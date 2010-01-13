#!/usr/bin/perl -U

#
#  Copyright (C) 1999-2001 Ricardo Ueda Karpischek
#
#  This is free software; you can redistribute it and/or modify
#  it under the terms of the version 2 of the GNU General Public
#  License as published by the Free Software Foundation.
#
#  This software is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this software; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
#  USA.
#

#
# Clara CGI
#
# This program may be used in many different ways.
#
# Running from the command line:
#
#   clara.pl -d
#
#       Daemon mode. Alternates a recog_doc run with sleeping 30 seconds.
#       In this case, recog_doc scans all book directories looking for
#       one page to be processed. Choose one page to process if it (1)
#       does not have a corresponding .html file (so it presumably was
#       never processed before) or (2) one of its symbols was revised
#       and the revision data is waiting processing.
#
#       "Process" one page means to invoke Clara OCR informing that
#       page as command-line parameter. Clara will load the patterns,
#       the page, and will execute one complete OCR run (load revision
#       data, classification, build and doubts generation).
#
#   clara.pl -p <book> <page>
#
#       Process the given page from the given book.
#
#   clara.pl -s <book>
#
#       Deletes .jpg files for which no corresponding .pbm file was found.
#
# Running from the web server:
#
#   PATH_INFO="/image/<book>/<page>"
#   PATH_INFO="/text/<book>/<page>"
#   PATH_INFO="/rev/<book>"
#   PATH_INFO="/img/<book>/<page>.jpg"
#   PATH_INFO=""
#

#
# CONFIGURATION SECTION - BEGIN
#

#
# Clara root
#
$CROOT = "/home/clara/www";

#
# base CGI URL for revision
#
$U = "/cgi-bin/clara";

#
# Descriptions of the books.
#
#$book[0] = '[French] Térèse D\'Avila, <I>Exclamations</I>, Paris, 1859 (trad. M. Bouix)';
#$book[1] = '[Portuguese] Dicionário de Cândido de Figueiredo (4a. ed.), Lisboa, 1925';
#$book[2] = '[Portuguese] Imre Simon\'s thesis, São Paulo, 1978';

#
# Subdirs of the books.
#
#$subdir[0] = "fondations";
#$subdir[1] = "cf";
#$subdir[2] = "imre";

#
# Language.
#
# Current possibilities are: 'en' (English) and 'br' (Portuguese).
#
$LANG = 'en';

#
# Clara options
#
$opt = '-W -R 10 -b -k 2,1.4,1.57,10,3.8,10,4,1';

#
# When nonzero, generates compressed session files.
#
$ZS = 0;

#
# CONFIGURATION SECTION - END
#

#
# Short Texts
#
if ($LANG eq 'en') {
    $OCR_RESULT = 'OCR Result';
    $REVIEW = 'Review';
    $PROT = 'Prototype of the Cooperative Revision';
    $DOC_IMAGE = 'Document Image';
    $IMAGE = 'image';
    $TRANSLIT = 'transliteration';
    $LAST_PROC = 'last processing';
    $STATUS = 'current status';
    $UNAVAL = 'unavailable';
}
else {
    $OCR_RESULT = 'Resultado do Reconhecimento';
    $PROT = 'Protótipo da Revisão Cooperativa';
    $REVIEW = 'Revisar';
    $DOC_IMAGE = 'Imagem de Documento';
    $IMAGE = 'imagem';
    $TRANSLIT = 'transliteração';
    $LAST_PROC = 'último processamento';
    $STATUS = 'estado atual';
    $UNAVAL = 'não disponível';
}

#
# extract CGI variables from input.
#
sub cgi_receive {
    if ($ENV{'REQUEST_METHOD'} eq "POST") {
        read(STDIN, $incoming, $ENV{'CONTENT_LENGTH'});
    }
    else {
        $incoming = $ENV{'QUERY_STRING'};
    }
    $incoming =~ s/%0D//gi;
}

#
# URL decode.
#
sub cgi_decode {
    local(@pairs) = split(/&/, $incoming);
    local($i,@chr);

    for ($i=0; $i<256; ++$i) {
        $chr[$i] = pack("C",$i);
    }
    foreach (@pairs) {
        local($name, $value) = split(/=/, $_);
        $name  =~ tr/+/ /;
        $value =~ tr/+/ /;
        $name  =~ s/%([A-F0-9][A-F0-9])/$chr[hex($1)]/gieo;
        $value =~ s/%([A-F0-9][A-F0-9])/$chr[hex($1)]/gieo;
        if (defined $FORM{$name}) {
            $FORM{$name} .= ";$value";
        }
        else {
            $FORM{$name} = $value;
        }
    }
}

#
#
#
sub html_hdr
{
    print "Content-type: text/html\n\n";
    print "<HTML><TITLE><HEAD>$_[0]</HEAD></TITLE>\n";
    print "<BODY BGCOLOR=D0D0D0>\n";
    print "<TABLE WIDTH=100% BORDER=1 BGCOLOR=#E2D3FC>\n";
    print "<TR><TD><CENTER><H1><BR>$_[0]<BR></H1></CENTER></TD>\n";
    print "</TR></TABLE>\n";

    print "<P><CENTER>\n";
    print "[<A href=faq.html>FAQ</A>]\n";
    print "[<A href=clara-tut.html>Tutorial</A>]\n";
    print "[<A href=clara-adv.html>User's Manual</A>]\n";
    print "[<A href=clara-dev.html>Developer's Guide</A>]\n";
    print "</CENTER>\n";
}

#
# Generate a random integer in the range 0..$_[0].
#
sub ri
{
    local($a);

    $a = rand($_[0]-1);
    ($b=$a) =~ s/^.*\././;
    $a =~ s/\..*$//;
    if ($b > 0.5) {
        ++$a;
    }
    return($a);
}

#
# Generate a revision form.
#
sub revform
{
    local(*D,$d,$s,$f,$n,$r,$p,$w);

    print "Content-type: text/html\n";
    print "Pragma: no-cache\n\n";
    print "<HTML><HEAD><TITLE>Revisão</TITLE></HEAD>\n";
    print "<BODY BGCOLOR=#D0D0D0>\n";
    $f0 = "";
    $s = $_[0];
    opendir(D,"${CROOT}/$s/doubts");
    for $d (readdir(D)) {
        if ($d =~ /^u\.[^.]*\.\d+\.pbm$/) {
            ($f,$f0) = ($d =~ /^u\.([^.]*)\.(\d+)\.pbm$/);
            system("$CROOT/bin/sclara -u $s/doubts/$d");
            last;
        }
    }
    closedir(D);

    # append to the revision file the submitted record (if any)
    if ($FORM{'TR'} ne "") {
        local($cmd,$d,$mc);

        ($d,$mc) = split('\.',$FORM{'F0'});

        #
        #   The "unused" fields are currently ignored by sclara.
        #
        system("$CROOT/bin/sclara","$s/doubts","unused",
               $ENV{'HTTP_HOST'},time(),
               'nobody','unused',"unused",$mc,$d,"$d.pbm",
               "$FORM{'TR'}");

        #$cmd  = "$CROOT/bin/sclara $s/doubts $d IP name user s_id ";
        #$cmd .= "-1 " . $mc;
        #$cmd .= " anonymous ignore $FORM{'TR'}";
        #open(Z,">/tmp/claralog");
        #print(Z "$FORM{'F0'}\n");
        #print(Z "$cmd\n");
        #close(Z);
        #system($cmd);
    }

    if ($f0 eq "") {

        print "<TABLE WIDTH=100% CELLPADDING=10 BORDER=1 BGCOLOR=#E2D3FC>\n";

        if ($LANG eq 'en') {
            print "<TR><TD><I><B>Revision completed</B></I>\n";
            print "The revision of this book is complete or we're waiting\n";
            print "new doubts to be generated by the next processing step\n";
            print "<P>Otherwise, <A HREF=$U> go back\n";
            print "to the main page </A>. Thanks!\n";
        }
        else {
            print "<TR><TD><I><B>Revisão concluída</B></I>\n";
            print "A revisão deste livro já foi concluída ouestamos\n";
            print "aguardando novas dúvidas que serão geradas pelo próximo\n";
            print "processamento das páginas\n";
            print "<P>Se desejar, <A HREF=$U> retorne\n";
            print "à página principal </A>. Obrigado!\n";
        }


        print "</TD></TR></TABLE>\n";
    }

    else {

        print "<TABLE WIDTH=100% CELLPADDING=10 BORDER=1 BGCOLOR=#E2D3FC>\n";

        if ($LANG eq 'en') {

            print "<TR><TD><I><B>Revision:</B></I>\n";

            print "See the image below. It's a region clipped from the\n";
            print "document, with one symbol grayed\n";

            print "<P>Fill the field <B>transliteration</B>\n";
            print "with the transliteration of the grayed symbol. For instance: if\n";
            print "it's the <I>image</I> of one <B>a</B> letter, then fill the field with\n";
            print "the <I>letter</I> <B>a</B>, and press the button <B>OK</B>.\n";

            print "<P>Alternatively you can <A HREF=$U> go back\n";
            print "to the main page </A> or\n";
            print "<A HREF=$U/rev/$s> get another symbol</A>\n";
            print "to review. Thanks!\n";
        }

        else {
            print "<TR><TD><I><B>Revisão:</B></I>\n";

            print "Observe a imagem abaixo. Ela corresponde\n";
            print "a uma região recortada do documento, com um símbolo\n";
            print "em destaque.\n";

            print "<P>Preencha o campo <B>transliteração</B>\n";
            print "com a transliteração do símbolo em destaque. Por exemplo: se\n";
            print "ele for a <I>imagem</I> de uma letra <B>a</B>, então escreva nele a\n";
            print "<I>letra</I> <B>a</B>, e em seguida pressione o botão <B>OK</B>.\n";

            print "<P>Ou então <A HREF=$U> Retorne à página principal\n";
            print "</A> ou <A HREF=$U/rev/$s> escolha outro símbolo\n";
            print "</A> para revisar. Obrigado!\n";
        }

        print "</TD></TR></TABLE>\n";

        print "<FORM METHOD=POST ACTION=$U/rev/$s>\n";

        print "<P><CENTER>\n";
        print "<TABLE BORDER=1 CELLPADDING=10 BGCOLOR=#79BEC6 WIDTH=100%>\n";

        print "<TR><TH WIDTH=20%><B>$IMAGE</B></TH>";
        print "<TD WIDTH=80%><IMG SRC=$U/img/$s/doubts/$f.$f0.jpg></TD>\n";
        print "</TR>\n";

        print "<TR><TH WIDTH=20%><B>$TRANSLIT</B></TH>";
        print "<TD WIDTH=80%><INPUT TYPE=TEXT NAME=TR SIZE=20>";
        print "<INPUT TYPE=SUBMIT VALUE=OK></TD>\n";
        print "</TR></TABLE></CENTER>\n";

        print "<INPUT TYPE=HIDDEN NAME=F0 VALUE=$f.$f0>\n";
        print "</FORM>\n";

    }

    print "</BODY></HTML>\n";
}

#
# Generates a web page that inserts the scanned image of a book page. The
# image is assumed to be in jpeg format.
#
sub image
{
    local($s,$f,$i,$p);

    print "Content-type: text/html\n\n";
    print "<HTML><HEAD><TITLE>$DOC_IMAGE</TITLE></HEAD>\n";
    print "<BODY BGCOLOR=#D0D0D0>\n";
    print "<TABLE WIDTH=100% BORDER=1 BGCOLOR=#E2D3FC>\n";
    print "<TR><TD><CENTER><H1><BR>$DOC_IMAGE</H1></CENTER></TD></TR></TABLE>\n";

    ($s,$f) = split("/",$_[0]);
    for ($i=0; ($i<=$#subdir) && ($subdir[$i] ne $s); ++$i) {}
    ($p=$f) =~ s/jpg$/pbm/;

    print "<P><TABLE WIDTH=100%><TR><TD WIDTH=5%></TD><TD WIDTH=90%>\n";

    if ($LANG eq 'en') {

        print "<P>This is the page <B>$f</B> from the book <B>\"$book[$i]</B>\".\n";

        print "<P>You can <A HREF=$U> go back\n";
        print "to the main page </A> or perhaps\n";
        print "<A HREF=$U/rev/$s> help reviewing </A>\n";
        print "this book.\n";

        print "<P><B>Remark:</B> This image was severely impoverished\n";
        print "to make possible a fast <I>download</I>. It's useful for\n";
        print "human visualization, not OCR.\n";

    }

    else {

        print "<P>Esta é a página <B>$f</B> do livro <B>\"$book[$i]</B>\".\n";

        print "<P>Quando desejar você poderá <A HREF=$U> Retornar\n";
        print "à página principal </A> ou eventualmente\n";
        print "<A HREF=$U/rev/$s> ajudar a revisão </A>\n";
        print "desse livro.\n";

        print "<P><B>Obs.</B> A qualidade da imagem abaixo foi severamente reduzida\n";
        print "para permitir um rápido <I>download</I>. Na forma em que está,\n";
        print "ela destina-se apenas a uma rápida visualização, e não ao OCR.\n";

    }

    print "</TD><TD WIDTH=5%></TD></TR></TABLE>\n";
    print "<P><CENTER><IMG SRC=$U/img/$s/$f.jpg></TD></CENTER>\n";
    print "<P></BODY></HTML>\n";
}

#
# Generates a web page containing the current OCR result for the specified
# page of the specified book.
#
sub text
{
    local($s,$f,$i,$p,*F);

    print "Content-type: text/html\n";
    print "Pragma: no-cache\n\n";

    print "<HTML><HEAD><TITLE>$OCR_RESULT</TITLE></HEAD>\n";
    print "<BODY BGCOLOR=#D0D0D0>\n";
    print "<TABLE WIDTH=100% BORDER=1 BGCOLOR=#E2D3FC>\n";
    print "<TR><TD><CENTER><H1><BR>$OCR_RESULT</H1></CENTER></TD></TR></TABLE>\n";

    ($s,$f) = split("/",$_[0]);
    for ($i=0; ($i<=$#subdir) && ($subdir[$i] ne $s); ++$i) {}
    ($p=$f) =~ s/jpg$/pbm/;

    print "<P><TABLE WIDTH=100%><TR><TD WIDTH=5%></TD><TD WIDTH=90%>\n";

    if ($LANG eq 'en') {
        print "<P>This is the current OCR result for the page <B>$f</B>\n";
        print "from the book <B>\"$book[$i]</B>\".\n";

        print "<P>You can <A HREF=$U> go back\n";
        print "to the main </A> or perhaps enter the\n";
        print "<A HREF=$U/rev/$s> revision form </A>\n";
        print "for this page.\n";
    }

    else {
        print "<P>Este é o resultado atual do reconhecimento da página <B>$f</B>\n";
        print "do livro <B>\"$book[$i]</B>\".\n";

        print "<P>Quando desejar você poderá <A HREF=$U> Retornar\n";
        print "à página principal </A> ou eventualmente entrar no\n";
        print "<A HREF=$U/rev/$s> formulário de revisão </A>\n";
        print "dessa página.\n";
    }

    print "</TD><TD WIDTH=5%></TD></TR></TABLE>\n";
    print "<P><TABLE WIDTH=100%><TR><TD BGCOLOR=#F0F0F0><PRE>\n";

    open(F,"$CROOT/$s/$f.html");
    while (<F>) {
        print;
    }
    close(F);

    print "</PRE></TD></TR></TABLE><P></BODY></HTML>\n";
}

#
# Generates the main web page containing the list of all books.
#
sub books
{
    local (*D,$d,$t,$j,$p);

    print "Content-type: text/html\n\n";

    print "<HTML><HEAD><TITLE>$PROT</TITLE></HEAD>\n";
    print "<BODY BGCOLOR=#D0D0D0>\n";

    print "<TABLE WIDTH=100% BORDER=1 BGCOLOR=#E2D3FC>\n";
    print "<TR><TD><CENTER><H1><BR>$PROT</H1></CENTER></TD></TR></TABLE>\n";

    print "<P><TABLE WIDTH=100%><TR><TD WIDTH=5%></TD><TD WIDTH=90%>\n";

    if ($LANG eq 'en') {
        print "<P>Welcome! This is a prototype for the cooperative revision\n";
        print "featured by <A HREF=http://www.claraocr.org/> Clara OCR </A>.\n";

        print "<P>The table below presents the books (*) included in this\n";
        print "prototype. Only few pages from each one were included. The table\n";
        print "has links to the digital images of the pages\n";
        print "(column <B>image</B>) and the current OCR result for\n";
        print "each page (column <B>text</B>)\n";

        print "<P>From the <B>$REVIEW</B> linked one can join the revision\n";
        print "effort as a volunteer to make the recognition result\n";
        print "become better\n";
    }

    else {
        print "<P>Bem-vindo! este é um protótipo da revisão cooperativa\n";
        print "baseado no <A HREF=http://www.claraocr.org>Clara OCR</A>.\n";

        print "<P>A tabela abaixo relaciona os livros * que constam do protótipo.\n";
        print "Foram incluídas apenas algumas páginas de cada um dos livros.\n";
        print "A tabela possui <I>links</I> que permitem obter a imagem digitalizada\n";
        print "(coluna <B>imagem</B>) e o resultado atual do reconhecimento de cada\n";
        print "página (coluna <B>texto</B>)\n";

        print "<P>Usando o link <B>$REVIEW</B> pode-se\n";
        print "colaborar no esforço voluntário de\n";
        print "revisão dos textos para melhorar o resultado do reconhecimento\n";
    }

    print "</TD><TD WIDTH=5%></TD></TR></TABLE>\n";

    print "<P><CENTER><TABLE BGCOLOR=#E0E0E0 BORDER=1 WIDTH=100%>\n";

    for ($i=0; $i<=$#book; ++$i) {

        $s = $subdir[$i];
        print "<TR>\n";
        print "<TD BGCOLOR=#79BEC6 COLSPAN=2>";
        print "<B>" . ($i+1) . ". $book[$i]</B></TD>";
        print "<TD BGCOLOR=#79BEC6 ALIGN=RIGHT>";
        print "<A HREF=$U/rev/$s>$REVIEW</A></TD>\n";
        print "</TR><TR>\n";
        print "<TR>\n";

        print "<TH>$IMAGE</B></TH>\n";
        print "<TH><B>$LAST_PROC</B></TH>\n";
        print "<TH><B>$STATUS</B></TH>\n";

        print "</TR><TR>\n";

        opendir(D,"$CROOT/$s");
        for $d (sort {$a <=> $b} readdir(D)) {
            local(*F,$db,$dt);

            if (($d !~ /^\./) && ($d !~ 'doubts') && ($d =~ /.pbm.gz$/)) {
                ($p = $d) =~ s/.pbm.gz$//;
                $t = $p . ".html";
                $j = $p . ".jpg";
                if (-e "$CROOT/$s/$t") {
                    open(F,"$CROOT/$s/$t");
                    #($db = <F>) =~ s/^.*?Symbols: //;
                    #$db =~ s/symbols//;
                    $db = '?';
                    ($dt = <F>) =~ s/^.*?at //;
                }
                else {
                    $db = $dt = '?';
                }
                print "<TD><A HREF=$U/image/$s/$p>$p</A></TD>\n";
                if ($dt eq '?') {
                    print "<TD><I>$UNAVAL</I></TD>\n";
                    print "<TD>?</TD>\n";
                }
                else {
                    print "<TD><A HREF=$U/text/$s/$p>$dt</A></TD>\n";
                    print "<TD>$db</TD>\n";
                }
                print "</TR><TR>\n";
            }
        }
        closedir(D);

        print "</TR><TD COLSPAN=4></TD><TR>\n";
    }

    print "</TR></TABLE></CENTER>\n";

    if ($LANG eq 'en') {
        print "<P>(*) <I>As far as we could verify, these books are out of\n";
        print "copyright, but in some cases we\'re reproducting materials\n";
        print "authorized by the copyright holder. The choice of the books\n";
        print "is based on availability and OCR results only.</I>\n";
    }

    else {
        print '(*) <I>Até onde pudemos verificar, estes livros estão com o copyright';
        print 'caduco, mas em alguns casos eles foram incluídos aqui';
        print 'através de uma autorização explícita. A escolha dos títulos';
        print 'baseou-se apenas nos resultados do OCR e no fato de possuirmos';
        print 'estes livros à mão';
    }

    print "<P></BODY></HTML>\n";
}

#
# Run a recognition step on one book page, specified on the command line
# or automatically chosen.
#
sub recog_doc
{
    local(*D,$d,$df,*E,$e,$C,$c,$f,$j,$p);

    # book and document from command line
    if ($#ARGV >= 2) {
        $b = $ARGV[1];
        $d = $ARGV[2];
    }

    # scan subtree to find a document to process
    else {

        # Clara home
        $b = "";
        opendir(C,$CROOT);
        for $c (readdir(C)) {
            if ((!($c =~ /^\./)) && (!($c =~ /bin/)) && (-d "$CROOT/$c")) {

                # book home
                opendir(D,"$CROOT/$c");
                for $f (readdir(D)) {

                    # a page with no corresponding .html: process
                    if ($f =~ /.pbm.gz$/) {
                        ($p = $f) =~ s/.pbm.gz$//;
                        if (!(-e "$CROOT/$c/$p.html")) {
                            $b = $c;
                            $d = $p;
                            last;
                        }
                    }
                }
                closedir(D);

                # doubts dir
                if (($b eq "") && (-d "$CROOT/$c/doubts")) {

                    opendir(E,"$CROOT/$c/doubts");
                    for $e (readdir(E)) {

                        # found a post
                        if ($e =~ /^\w+\.\d+\.\d+\.\w+$/) {
                            $b = $c;
                            ($d) = ($e =~ /^(\w+)\./);
                            last;
                        }
                    }
                    closedir(E);
                }
            }
            last if ($b ne "");
        }
        closedir(C);

        # no data to process
        if ($b eq "") {
            return(0);
        }
    }

    # process the document
    $W = "$CROOT/$b";
    print("going to process document $W/$d\n");
    chdir($W);
    $U .= "/rev/$b/$d";
    if (-e "$d.pbm") {
        $df = "$d.pbm";
    }
    elsif (-e "$d.pbm.gz") {
        $df = "$d.pbm.gz";
    }
    #system("$CROOT/bin/clara -z -f $d.pbm -U $U -r -b >/dev/null");
    #system("$CROOT/bin/clara -z -f $df -W -R 30 -b >/dev/null");
    if ($ZS) {
        $opt .= " -z";
    }
    system("$CROOT/bin/clara -f $df $opt >/dev/null");

    # convert revision bitmaps
    opendir(D,"$W/doubts");
    for $f (readdir(D)) {
        if ($f =~ /^u\..*?\.pbm$/) {
            ($j=$f) =~ s/pbm$/jpg/;
            $j =~ s/^u.//;
            system("sh -c 'convert doubts/$f doubts/$j'");
        }
    }
    closedir(D);
    return(1);
}

#
# The program begins here.
#

#
# Enter daemon mode.
#
if (($#ARGV >= 0) && ($ARGV[0] eq "-d")) {
    while (1) {
        if (recog_doc() == 0) {
            print("sleeping\n");
            sleep(30);
        }
    }
}

#
# Run the OCR to recognize document.
#
if (($#ARGV >= 0) && ($ARGV[0] eq "-p")) {
    &recog_doc();
    exit(0);
}

#
# Synchronize images with doubts. This code assumes that no
# concurrent document reprocessing is running.
#
if (($#ARGV >= 0) && ($ARGV[0] eq "-s")) {
    local(*D,*E,$d,$e,%c);

    # cache doubts, seen or unseen
    $b = $ARGV[1];
    opendir(D,"$CROOT/$b/doubts");
    for $d (readdir(D)) {
        if ($d =~ /^[us]\..*?\.pbm$/) {
            $d =~ s/^..//;
            $d =~ s/pbm$/jpg/;
            $c{$d} = 1;
        }
    }
    closedir(D);

    # unlink images
    opendir(D,"$CROOT/$b/doubts");
    for $d (readdir(D)) {
        if (($d =~ /jpg$/) && ($c{$d} != 1)) {
            unlink("$CROOT/$b/doubts/$d");
        }
    }
    closedir(D);
    exit(0);
}

#
# read CGI variables.
#
&cgi_receive();
&cgi_decode();

#
# Generates web page including the image of the given book page
#
if ($ENV{'PATH_INFO'} =~ /^\/image/) {
    ($fn) = ($ENV{'PATH_INFO'} =~ /^\/image\/(.*)$/);
    &image($fn);
}

#
# Generate web page including the current OCR result.
#
elsif ($ENV{'PATH_INFO'} =~ /^\/text/) {
    ($fn) = ($ENV{'PATH_INFO'} =~ /^\/text\/(.*)$/);
    &text($fn);
}

#
# Generates HTML page with revision form for the specified book.
#
elsif ($ENV{'PATH_INFO'} =~ /^\/rev/) {
    ($fn) = ($ENV{'PATH_INFO'} =~ /^\/rev\/(.*)$/);
    &revform($fn);
}

#
# Outputs the shrinked image of the book page.
#
elsif ($ENV{'PATH_INFO'} =~ /^\/img/) {
    local(*F);

    print "Content-type: image/jpeg\n\n";
    ($fn) = ($ENV{'PATH_INFO'} =~ /^\/img\/(.*)$/);
    open(F,"$CROOT/$fn");
    while (sysread(F,$b,1000) > 0) {
        print $b;
    }
}

#
#
#
elsif ($ENV{'PATH_INFO'} =~ /^\/list/) {

    &html_hdr("Clara OCR mailing lists");

    print "<P>Thank you for using our services!\n";

    print "<P>You requested to <B>\n";
    print ($FORM{'OP'} eq 'U') ? 'un' : '';
    print "subscribe</B> the electronic address <B>$FORM{'ADDR'}</B>\n";
    print ($FORM{'OP'} eq 'S') ? 'to' : 'from';
    print ' the Clara OCR <B>';
    print ($FORM{'LIST'} eq 'A') ? 'Announce' : 'Developers\'';
    print "</B> list. The electronic address <B>$FORM{'ADDR'}</B> was\n";
    print ($FORM{'OP'} eq 'S') ? 'added to' : 'removed from';
    print " that list and a confirmation message was sent to <B>$FORM{'ADDR'}</B>.\n";

    print "<P><HR></BODY></HTML>";
}

#
# Generates main web page with the list of books.
#
else {
    &books();
}
