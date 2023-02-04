desktop = meson.project_name() + '.desktop'
gschema = 'org.gtk.' + meson.project_name() + '.gschema.xml'

run_command('sh', '-c', 'echo "[Desktop Entry]\nName=Gmf\nComment=File manager\nExec=gmf %F\nIcon=system-file-manager\nTerminal=false\nType=Application\nCategories=GTK;Utility;\nMimeType=inode/directory;" > desktop', check: true)
configure_file(input: 'desktop', output: desktop, copy: true, install: true, install_dir: join_paths('share', 'applications'))

run_command('sh', '-c', 'echo \'<?xml version="1.0" encoding="UTF-8"?>\n<schemalist gettext-domain="gmf">\n  <schema id="org.gtk.gmf" path="/org/gtk/gmf/">\n    <key name="dark" type="b">\n      <default>false</default>\n    </key>\n    <key name="icon-size" type="u">\n      <default>48</default>\n    </key>\n    <key name="opacity" type="u">\n      <default>100</default>\n    </key>\n    <key name="preview" type="b">\n      <default>true</default>\n    </key>\n    <key name="theme" type="s">\n      <default>"none"</default>\n    </key>\n    <key name="icon-theme" type="s">\n      <default>"none"</default>\n    </key>\n    <key name="width" type="u">\n      <default>700</default>\n    </key>\n    <key name="height" type="u">\n      <default>350</default>\n    </key>\n  </schema>\n</schemalist>\' > gschema', check: true)
configure_file(input: 'gschema', output: gschema, copy: true, install: true, install_dir: join_paths('share', 'glib-2.0/schemas'))

meson.add_install_script('sh', '-c', 'glib-compile-schemas /usr/share/glib-2.0/schemas')
meson.add_install_script('sh', '-c', 'update-desktop-database -q /usr/share/applications')
