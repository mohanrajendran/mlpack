def config_doit(sysentry, files, params):
  script = files["script"].single(Types.SCRIPT)
  outdir = sysentry.bin_dir("arch", "kernel", "compiler", "cflags")
  indir = sysentry.sys.source_dir + "/fastlib"
  sysentry.command("%s --genfiles_dir=%s --source_dir=%s --cflags=%s" % (
      sq(script), sq(outdir), sq(indir), sq(params["cflags"])))
  gen_headers = ["base/basic_types.h"]
  return [(Types.HEADER,
           sysentry.file(h, "arch", "kernel", "compiler", "cflags"))
          for h in gen_headers]

customrule(
    name = "config_headers",
    dependencies =
        {"script": [sourcerule(Types.SCRIPT, "../../script/config.py")],
         "sources": sourcerules(Types.ANY, lglob("/config/*.c"))},
    doit_fn = config_doit)

librule(
    sources = ["common.c", "debug.c", "cc.cc", "ccmem.cc", "otrav.cc"],
    headers = ["ansi_colors.h", "base.h", "common.h", "compiler.h", "debug.h",
               "cc.h", "ccmem.h", "otrav.h", "otrav_impl.h", "fortran.h",
               "test.h", "deprecated.h", ":config_headers"])
