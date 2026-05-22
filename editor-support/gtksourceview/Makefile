#stolen/adapted from http://github.com/nddrylliog/ooc-gtksourceview

install:
	mkdir -p ~/.local/share/gtksourceview-2.0/language-specs
	mkdir -p ~/.local/share/mime/packages
	cp -f ceramic.lang ~/.local/share/gtksourceview-2.0/language-specs
	cp -f ceramic.xml ~/.local/share/mime/packages
	cd ~/.local/share && update-mime-database mime

uninstall:
	rm ~/.local/share/gtksourceview-2.0/language-specs/ceramic.lang
	rm ~/.local/share/mime/packages/ceramic.xml
	cd ~/.local/share && update-mime-database mime
