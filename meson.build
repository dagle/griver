project ('misc', 'c')

source_c = [
  'giver.c',
  ]

source_h = [
  'giver.h',
  ]

deps = [
  dependency('gobject-2.0'),
  dependency('wayland-client')
]

wl_mod = import('unstable-wayland')
river_layout = wl_mod.scan_xml('river-layout-v3.xml')
river_status = wl_mod.scan_xml('river-status-unstable-v1.xml')

filter_lib = library('giver', source_c, river_layout, river_status, dependencies : deps)
gnome = import('gnome')

if true
  gir_args = [
    '--quiet',
    '--warn-all',
    ]

  filter_glib_gir = gnome.generate_gir(
    filter_lib,
    sources: source_c + source_h,
    namespace: 'Giver',
    nsversion: '0.1',
    symbol_prefix: ['g_river'],
    includes: [ 'GObject-2.0'],
    dependencies: deps,
    extra_args: gir_args,
    fatal_warnings: true,
    )
else
  # Just warn and quit
  json_glib_gir = []
endif
