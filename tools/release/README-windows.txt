nanozip-re @VERSION@ for Windows (@ARCH@)
==========================================

nanozip-re is a reimplementation of NanoZip 0.09a (Sami Runsas): it reads
every NanoZip archive and writes the same archives the original writes.
Project page: https://github.com/YadeWira/nanozip-re

Files
-----
nz-re.exe     the command line program (use it like the original nz.exe)
nz_w32c.sfx   the self-extracting archive stub of NanoZip 0.09a, unchanged;
              `nz-re w32c` needs it next to nz-re.exe
README.txt    this file
LICENSE.txt   the license of nanozip-re (MIT); nz_w32c.sfx is the original
              author's and is not covered by it

Quick start (in a Command Prompt or PowerShell window, in this folder)
-----------
nz-re a archive.nz file1 file2      compress (default compressor -co)
nz-re a -cc -r archive.nz folder    best compression, a whole folder
nz-re x archive.nz                  extract
nz-re l archive.nz                  list
nz-re t archive.nz                  test
nz-re w32c archive files...         self-extracting archive.exe
nz-re help                          all the options

To run it from any folder, add this folder to your PATH, or copy nz-re.exe
and nz_w32c.sfx to a folder that is already in it.

Notes
-----
- The x86 build runs on Windows XP SP3 and later, the x64 build on Vista
  and later (64-bit Windows). The x64 build is the faster one; use the x86
  build on 32-bit Windows. On 64-bit Windows the x86 build can use up to
  4 GB of memory, like the original 32-bit nz.exe.
- File names are stored in the active code page, as the original does:
  a name the code page cannot represent (for example Cyrillic on a
  Western Windows) is reported "Cannot open:" and left out of the
  archive, by both programs.

--------------------------------------------------------------------------

nanozip-re @VERSION@ para Windows (@ARCH@)

nanozip-re es una reimplementación de NanoZip 0.09a (Sami Runsas): lee
cualquier archivo de NanoZip y escribe los mismos archivos que el original.

nz-re.exe es el programa de línea de comandos (se usa como el nz.exe
original); nz_w32c.sfx es el stub autoextraíble original de NanoZip 0.09a,
sin cambios, y `nz-re w32c` lo necesita junto a nz-re.exe. Los ejemplos de
arriba valen igual; `nz-re help` lista todas las opciones. Requiere Windows
XP SP3 o posterior (x86) o Vista o posterior (x64); en Windows de 64 bits
conviene la versión x64.
