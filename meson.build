project ('griver', 'c',
  version : '0.1')

source_c = [
  'griver-context.c',
  'griver-output.c',
  ]

source_h = [
  'griver-context.h',
  'griver-output.h',
  ]

deps = [
  dependency('gobject-2.0'),
  dependency('wayland-client')
]

wl_mod = import('unstable-wayland')
river_layout = wl_mod.scan_xml('protocol/river-layout-v3.xml')
river_status = wl_mod.scan_xml('protocol/river-status-unstable-v1.xml')
river_control = wl_mod.scan_xml('protocol/river-control-unstable-v1.xml')

install_headers(source_h)

griver = library('griver', source_c, river_layout, river_status, river_control, 
  dependencies : deps, install: true)

gir_args = [
  '--quiet',
  '--warn-all',
  ]

gnome = import('gnome')

griver_glib_gir = gnome.generate_gir(
  griver,
  sources: source_c + source_h,
  namespace: 'Griver',
  nsversion: '0.1',
  symbol_prefix: ['g_river', 'griver'],
  includes: [ 'GObject-2.0'],
  dependencies: deps,
  extra_args: gir_args,
  fatal_warnings: true,
  install: true,
  )
