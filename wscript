from os import popen
import Utils

srcdir = "."
blddir = "build"
VERSION = "0.0.1"

def set_options(opt):
  opt.tool_options("compiler_cxx")

def configure(conf):
  conf.check_tool("compiler_cxx")
  conf.check_tool("node_addon")
  pkg_config = conf.find_program('pkg-config', var='PKG_CONFIG', mandatory=True)
  sqlite3_libdir = popen("%s sqlite3 --libs-only-L" % pkg_config).readline().strip();
  if sqlite3_libdir == "":
    sqlite3_libdir = "/usr/lib"
  sqlite3_libdir = sqlite3_libdir.replace("-L", "")
  sqlite3_includedir = popen("%s sqlite3 --cflags-only-I" % pkg_config).readline().strip();
  if sqlite3_includedir == "":
    sqlite3_includedir = "/usr/include"
  sqlite3_includedir = sqlite3_includedir.replace("-I", "")
  conf.env.append_value("LIBPATH_SQLITE3", sqlite3_libdir)
  conf.env.append_value('CPPPATH_SQLITE3', sqlite3_includedir)
  conf.env.append_value("LIB_SQLITE3", "sqlite3")

def build(bld):
  obj = bld.new_task_gen("cxx", "shlib", "node_addon")
  obj.target = "sqlite3_bindings"
  obj.source = "sqlite3_bindings.cc"
  obj.uselib = "SQLITE3"
