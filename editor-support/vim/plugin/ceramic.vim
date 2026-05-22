nmap <Leader>cl :LibCeramicModule<SPACE>
nmap <Leader>cn :LibCeramicNewModule<SPACE>
nmap <Leader>cN :LibCeramicNewModuleDir<SPACE>
nmap <Leader>c0 :LibCeramicAlternate 0<CR>
nmap <Leader>c1 :LibCeramicAlternate 1<CR>
nmap <Leader>c2 :LibCeramicAlternate 2<CR>
nmap <Leader>c3 :LibCeramicAlternate 3<CR>
nmap <Leader>c4 :LibCeramicAlternate 4<CR>
nmap <Leader>c5 :LibCeramicAlternate 5<CR>
nmap <Leader>c6 :LibCeramicAlternate 6<CR>
nmap <Leader>c7 :LibCeramicAlternate 7<CR>
nmap <Leader>c8 :LibCeramicAlternate 8<CR>
nmap <Leader>c9 :LibCeramicAlternate 9<CR>

if !exists("g:LibCeramic")
    let g:LibCeramic = "/usr/local/lib/lib-ceramic"
endif

if !exists("g:LibCeramicAlternates")
    let g:LibCeramicAlternates = ["/usr/local/lib/lib-ceramic"]
endif

function! s:unique(list)
    let dict = {}
    for value in a:list
        let dict[value] = 1
    endfor
    return sort(keys(dict))
endfunction

function! CeramicCompleteLibraryModule(arglead, cmdline, cursorpos)
    let modulesl = []
    let modulelead = substitute(a:arglead, "\\.", "/", "g")
    let modules = globpath(g:LibCeramic, modulelead . "*")
    if modules != ""
        let modulesl = split(modules, "\n")
        let modulesl = filter(modulesl, 'getftype(v:val) == "dir" || matchstr(v:val, "\\.ceramic$") != ""')
        let modulesl = map(modulesl, 'substitute(v:val, "\\(\\..*\\)\\?\\.ceramic$", "", "")')
        let modulesl = map(modulesl, 'substitute(v:val, "^\\V" . escape(g:LibCeramic, "\\") . "\\v[/\\\\]", "", "")')
        let modulesl = s:unique(modulesl)
        let modulesl = map(modulesl, 'substitute(v:val, "/\\|\\\\", ".", "g")')
    endif
    return modulesl
endfunction

command! -nargs=1 -complete=customlist,CeramicCompleteLibraryModule LibCeramicModule :call GoToLibCeramicModule("<args>")
command! -nargs=1 -complete=customlist,CeramicCompleteLibraryModule LibCeramicNewModule :call CreateLibCeramicModule("<args>")
command! -nargs=1 -complete=customlist,CeramicCompleteLibraryModule LibCeramicNewModuleDir :call CreateLibCeramicModuleDir("<args>")
command! -nargs=1 LibCeramicAlternate :call SelectLibCeramicAlternate(<args>)

function! CeramicModuleFileNames(path)
    let names = [a:path . ".ceramic"]
    let oses = ["unix", "windows", "linux", "freebsd", "macosx"]
    let cpus = ["x86", "ppc", "arm"]
    let bits = ["32", "64"]
    for bit in bits
        let names += [a:path . "." . bit . ".ceramic"]
    endfor
    for cpu in cpus
        let names += [a:path . "." . cpu . ".ceramic"]
        for bit in bits
            let names += [a:path . "." . cpu . "." . bit . ".ceramic"]
        endfor
    endfor
    for os in oses
        let names += [a:path . "." . os . ".ceramic"]
        for bit in bits
            let names += [a:path . "." . os . "." . bit . ".ceramic"]
        endfor
        for cpu in cpus
            let names += [a:path . "." . os . "." . cpu . ".ceramic"]
            for bit in bits
                let names += [a:path . "." . os . "." . cpu . "." . bit . ".ceramic"]
            endfor
        endfor
    endfor
    return names
endfunction

function! FindCeramicModuleFile(path)
    let basename = substitute(a:path, "^.*[/\\\\]", "", "")
    let searchnames = CeramicModuleFileNames(a:path)
    let searchnames += CeramicModuleFileNames(a:path . "/" . basename)
    for name in searchnames
        if getftype(name) != ""
            return name
        endif
    endfor
    return ""
endfunction

function! GoToLibCeramicModule(module)
    let modulefile = FindCeramicModuleFile(g:LibCeramic . "/" . substitute(a:module, "\\.", "/", "g"))
    if modulefile == ""
        echo "Library module" modulefile "not found"
    else
        exe "edit " fnameescape(modulefile)
    endif
endfunction

function! SelectLibCeramicAlternate(n)
    echo "Setting g:LibCeramic to" get(g:LibCeramicAlternates, a:n, g:LibCeramic)
    let g:LibCeramic = get(g:LibCeramicAlternates, a:n, g:LibCeramic)
endfunction

function! CreateLibCeramicModule(module)
    let modulename = g:LibCeramic . "/" . substitute(a:module, "\\.", "/", "g") . ".ceramic"
    let modulepath = substitute(modulename, "[/\\\\][^/\\\\]*$", "", "")
    exe "silent !mkdir -p " shellescape(modulepath)
    exe "edit " fnameescape(modulename)
endfunction

function! CreateLibCeramicModuleDir(module)
    let modulepath = g:LibCeramic . "/" . substitute(a:module, "\\.", "/", "g")
    let basename = substitute(a:module, "^.*\\.", "", "")
    exe "silent !mkdir -p " shellescape(modulepath)
    exe "edit " fnameescape(modulepath . "/" . basename . ".ceramic")
endfunction
