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

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : true)

wl_mod = import('unstable-wayland')
river_layout = wl_mod.scan_xml('protocol/river-layout-v3.xml')

install_headers(source_h, subdir : 'griver')

griver = library('griver', source_c, river_layout,
  dependencies : deps + m_dep, install: true)

pkg = import('pkgconfig')

pkg.generate(griver)

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
