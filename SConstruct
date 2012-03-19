# -*- python -*-

Command("clara_marshalers.c", "clara_marshalers.list", "glib-genmarshal --body ${SOURCES} >${TARGETS}")
Command("clara_marshalers.h", "clara_marshalers.list", "glib-genmarshal --header ${SOURCES} >${TARGETS}")

common_sources = [
    'clara_marshalers.c',
    'clara.c',
    'data.c',
    'skel.c',
    'event.c',
    'symbol.c',
    'pattern.c',
    'pbm2cl.c',
    'cml.c',
    'welcome.c',
    'util.c',
    'alphabet.c',
    'revision.c',
    'build.c',
    'consist.c',
    'pgmblock.c',
    'preproc.c',
    'obd.c',
    'html.c',
    'redraw.c',
    'docView.c',
    ]

env = Environment()

env.Replace(CCCOMSTR   = " [CC] $TARGET",
            LINKCOMSTR = " [LD] $TARGET")

#env.Append(CFLAGS = ["-fprofile-arcs","-ftest-coverage"])
#env.Append(LINKFLAGS = ["-fprofile-arcs","-ftest-coverage"])

env.ParseConfig('pkg-config --cflags --libs gtk+-2.0 webkit-1.0 imlib2')
env.Append(CFLAGS = ["-g","-Wall","-O0", "-Werror"])

env.Append(CFLAGS = ['-DHAVE_POPEN','-DMEMCHECK','-DFUN_CODES','-DHAVE_SIGNAL','-DTEST','-DNO_RINTF'])
env.Program("clara", common_sources + ["clara-main.c"])
env.Program("clara-tests", common_sources + ["clara-tests.c"])

# env.Program("manager", ["manager.c"])

Command("TAGS", common_sources + ["common.h"], "etags ${SOURCES}")
