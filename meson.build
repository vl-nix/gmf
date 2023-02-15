project('gmf', 'c', version: '23.2', meson_version: '>= 0.50.0', default_options: [ 'warning_level=2', 'c_std=gnu11'])

c_args = ['-DVERSION="' + meson.project_version() + '"']

cc = meson.get_compiler('c')

test_cflags = [
    '-Wconversion',
    '-fno-strict-aliasing',
    '-Wpointer-arith',
    '-Wmissing-declarations',
    '-Wmissing-prototypes',
    '-Wimplicit-function-declaration',
    '-Wformat=2',
    '-Wformat-security',
    '-Wnested-externs',
    '-Wold-style-definition',
    '-Wundef',
    '-Wunused',
    '-Wcast-align',
    '-Wmissing-noreturn',
    '-Wmissing-format-attribute',
    '-Wmissing-include-dirs',
    '-Wlogical-op',
    '-Wignored-qualifiers',
    '-Wno-discarded-qualifiers',
    '-Werror=implicit',
    '-Werror=nonnull',
    '-Werror=init-self',
    '-Werror=main',
    '-Werror=missing-braces',
    '-Werror=sequence-point',
    '-Werror=return-type',
    '-Werror=trigraphs',
    '-Werror=array-bounds',
    '-Werror=write-strings',
    '-Werror=address',
    '-Werror=int-to-pointer-cast',
    '-Werror=pointer-to-int-cast',
    '-Werror=empty-body',
    '-Werror=write-strings'
]

# common_cflags = cc.get_supported_arguments(test_cflags)
# c_args += common_cflags

subdir('data')

c = run_command('sh', '-c', 'for file in src/*.h src/*.c; do echo $file; done', check: true )
src = c.stdout().strip().split('\n')

deps  = [dependency('gtk+-3.0', version: '>= 3.22')]

executable(meson.project_name(), src, dependencies: deps, c_args: c_args, install: true)
