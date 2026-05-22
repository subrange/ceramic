Place the syntax/ceramic.vim file inside ~/.vim/syntax for syntax highlighting,
and the plugin/ceramic.vim file inside ~/.vim/plugin for ceramic-specific commands.

Add the following line to your .vimrc file to use the syntax highlighter:
--
au BufRead,BufNewFile *.ceramic set filetype=ceramic
--

If you've installed Ceramic to a different path from /usr/local/lib, set
the g:LibCeramic variable for the plugin:
--
let g:LibCeramic = "/path/to/lib-ceramic"
--

The ceramic plugin adds the following ex-mode commands:

:LibCeramicModule <module> [<platform>]
    Opens the ceramic source file for the library module <module> under
    the g:LibCeramic directory. <module> is specified as a dotted path, as
    in a ceramic "import" statement. If there are multiple platform-specific
    source files for the module, one is chosen arbitrarily.
    Example:
        :LibCeramicModule io.files
:LibCeramicAlternate <n>
    Sets g:LibCeramic to the <n>th element of the g:LibCeramicAlternates
    variable. This makes it easy to navigate between different Ceramic
    projects.
    Example:
        :let g:LibCeramicAlternates = ["/usr/local/lib/lib-ceramic", "~/ceramicproject"]
        :LibCeramicAlternate 1
        ...do work in ~/ceramicproject
        :LibCeramicAlternate 0

The ceramic plugin adds the following command-mode keyboard commands:

\cl       short for :LibCeramicModule<SPACE>
\c<n>     short for :LibCeramicAlternate <n> (for <n> between 0 and 9)

If you've changed the mapleader character, that character will be used instead
of backslash (\) for the above keyboard commands.
