# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-
VERSION='0.1'
APPNAME='WSPROXY'

from waflib import Build, Logs, Utils, Task, TaskGen, Configure

def options(opt):
    opt.add_option('--debug',action='store_true',default=False,dest='debug',help='''debugging mode''')
    opt.add_option('--test', action='store_true',default=False,dest='_test',help='''build unit tests''')

    # if Utils.unversioned_sys_platform () == "darwin":
    #     pass

    opt.load('compiler_c compiler_cxx')
    opt.load('boost', tooldir=['waf-tools'])

def configure(conf):
    conf.load("compiler_c compiler_cxx")

    if conf.options.debug:
        conf.define ('WSP_DEBUG', 1)
        conf.add_supported_cxxflags (cxxflags = ['-O0',
                                                 '-std=c++11',
                                                 '-std=c++0x',
                                                 '-D_WEBSOCKETPP_CPP11_STL_',
                                                 '-Wall',
                                                 '-Wno-unused-variable',
                                                 '-g3',
                                                 '-Wno-unused-private-field', # only clang supports
                                                 '-fcolor-diagnostics',       # only clang supports
                                                 '-Qunused-arguments'         # only clang supports
                                                 ])
    else:
        conf.add_supported_cxxflags (cxxflags = ['-O3', 
                                                 '-std=c++11',
                                                 '-std=c++0x',
                                                 '-D_WEBSOCKETPP_CPP11_STL_',
                                                 '-g'])

    # conf.write_config_header('config.h')

    if conf.options._test:
        conf.define ('_TESTS', 1)
        conf.env.TEST = 1

    conf.load('boost')
    conf.check_boost(lib='system')

def build (bld):
    bld (
        target = "ws_proxy",
        features = ["cxx", "cxxprogram"],
        source = bld.path.ant_glob(['ws_proxy.cpp']),
        use = 'BOOST BOOST_SYSTEM',
        includes = '. ./websocketpp'
        )

@Configure.conf
def add_supported_cxxflags(self, cxxflags):
    """
    Check which cxxflags are supported by compiler and add them to env.CXXFLAGS variable
    """
    self.start_msg('Checking allowed flags for c++ compiler')

    supportedFlags = []
    for flag in cxxflags:
        if self.check_cxx (cxxflags=[flag], mandatory=False):
            supportedFlags += [flag]

    self.end_msg (' '.join (supportedFlags))
    self.env.CXXFLAGS += supportedFlags
