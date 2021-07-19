bindir = join_paths(get_option('prefix'), get_option('bindir'))
exec_path = bindir + '/' + meson.project_name()

desktop_conf = configuration_data()

set_desktop = [
  ['NAME', 'Gmf'],
  ['COMMENT', 'File manager'],
  ['EXEC', exec_path],
  ['ICON', 'system-file-manager'],
  ['TERMINAL', 'false'],
  ['TYPE', 'Application'],
  ['CATEGORIES', 'GTK;Utility;']
]

foreach props: set_desktop
  desktop_conf.set(props[0], props[1])
endforeach

desktop = meson.project_name() + '.desktop'

configure_file(
  input: 'desktop',
  output: desktop,
  configuration: desktop_conf,
  install: true,
  install_dir: join_paths('share', 'applications')
)
