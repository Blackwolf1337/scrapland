# Python 1.5.2

Unchanged files of the official release, https://www.python.org/ftp/python/src/py152.tgz
(sha256 c6651472e64b177bd71f5ade29b65b784faa7ca121430d735f6426bc52ab6265), license in COPYRIGHT.

- `Include/`: all headers. The hook DLL uses them with the stand-in `config/config.h` (layout of the MSVC build
  in Scrap.exe).
- `Objects/`, `Python/`, `Parser/`, `PC/` and the built-in modules of Scrap.exe in `Modules/` (errno, math, new,
  regex, signal, strop, _locale, marshal and imp are in Python/): for the standalone build, stage 2 of
  docs/standalone.md.
