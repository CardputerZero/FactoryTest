import os
import subprocess

Import('env')
with open(env['PROJECT_TOOL_S']) as f:
    exec(f.read())

toolchain_sysroot = os.environ.get('CONFIG_TOOLCHAIN_SYSROOT', '')
toolchain_multiarch = env.get('GCC_DUMPMACHINE', 'aarch64-linux-gnu')
toolchain_lib_path = os.path.join(
    toolchain_sysroot, 'usr', 'lib', toolchain_multiarch
) if toolchain_sysroot else ''


def config_enabled(name):
    return os.environ.get(name, '').lower() in ('1', 'y', 'yes', 'true')


is_desktop = config_enabled('CONFIG_FACTORY_TEST_USE_DESKTOP')

for config_header_name in ('global_config.h', 'lvgl_config.h'):
    config_header = os.path.join(
        env['PROJECT_PATH'], 'build', 'config', config_header_name
    )
    if os.path.exists(config_header):
        with open(config_header, encoding='utf-8') as config_file:
            config_content = config_file.read()
        updated_config_content = config_content.replace('#define DEBUG 1\n', '')
        if updated_config_content != config_content:
            with open(config_header, 'w', encoding='utf-8') as config_file:
                config_file.write(updated_config_content)


SRCS = append_srcs_dir(ADir('src'))
INCLUDE = [
    ADir('src'),
    ADir('src/app'),
    ADir('src/logger'),
    ADir('src/model'),
    ADir('src/platform'),
    ADir('src/reactive'),
    ADir('src/serialization'),
    ADir('src/view'),
    ADir('src/view/screens'),
    ADir('src/view/widgets'),
    ADir('src/viewmodel'),
    os.path.join(
        os.environ['SDK_PATH'],
        'components',
        'utilities',
        'party',
        'fmt',
        'include',
    ),
    os.path.join(os.environ['EXT_COMPONENTS_PATH'], 'Miniaudio', 'include'),
]
PRIVATE_INCLUDE = []
REQUIREMENTS = [
    'lvgl_component',
    'Miniaudio',
    'pthread',
    'dl',
    'freetype',
    'png16',
    'jpeg',
    'z',
    'm',
] + ['libyaml', 'libcjson']
if toolchain_sysroot:
    REQUIREMENTS += [':libfmt.so.10']
if is_desktop:
    REQUIREMENTS += pkg_config_ldflags('sdl2')
else:
    REQUIREMENTS += [
        'drm',
        'libcamera',
        'libcamera-base',
        'libnm',
        'libgio-2.0',
        'libgobject-2.0',
        'libglib-2.0',
        'gio-2.0',
        'gobject-2.0',
        'glib-2.0',
        'libudev',
        'libiperf',
        'libsctp',
        'liblirc',
        'libgpiod',
        'libserialport',
    ]
REQUIREMENTS = list(dict.fromkeys(REQUIREMENTS))
STATIC_LIB = []
DYNAMIC_LIB = []

desktop_dependency_includes = []
if is_desktop:
    try:
        desktop_include_flags = subprocess.check_output(
            ['pkg-config', '--cflags-only-I', 'sdl2', 'freetype2', 'libpng'],
            text=True,
        ).split()
        desktop_dependency_includes = [
            include_flag[2:]
            for include_flag in desktop_include_flags
            if include_flag.startswith('-I')
        ]
        INCLUDE += desktop_dependency_includes
    except (OSError, subprocess.CalledProcessError):
        pass
DEFINITIONS = [
    '-DFACTORY_TEST_SCONS_BUILD',
    '-std=c++17',
    '-g',
]
if not toolchain_sysroot:
    DEFINITIONS += ['-DFMT_HEADER_ONLY']
DEFINITIONS_PRIVATE = []
LDFLAGS = []
LINK_SEARCH_PATH = []
STATIC_FILES = [(ADir('assets'), 'assets')]

factory_test_config_header = os.path.join(
    env['PROJECT_PATH'], 'build', 'config', 'factory_test_config.h'
)
factory_test_config_lines = [
    '#pragma once\n\n',
    '#define USE_DESKTOP {}\n'.format(1 if is_desktop else 0),
]
factory_test_config_content = ''.join(factory_test_config_lines)
if (not os.path.exists(factory_test_config_header)
        or open(factory_test_config_header, encoding='utf-8').read()
        != factory_test_config_content):
    with open(factory_test_config_header, 'w', encoding='utf-8') as config_file:
        config_file.write(factory_test_config_content)
if toolchain_sysroot:
    INCLUDE += [
        os.path.join(toolchain_sysroot, 'usr', 'include'),
        os.path.join(toolchain_sysroot, 'usr', 'include', 'freetype2'),
        os.path.join(toolchain_sysroot, 'usr', 'include', 'libpng16'),
        os.path.join(toolchain_sysroot, 'usr', 'include', toolchain_multiarch),
    ]
    LINK_SEARCH_PATH += [toolchain_lib_path]
    LDFLAGS += [
        '-Wl,-rpath-link,{}'.format(toolchain_lib_path),
        '-B{}'.format(toolchain_lib_path),
    ]
if not is_desktop:
    INCLUDE += [
        os.path.join(toolchain_sysroot, 'usr', 'include', 'glib-2.0'),
        os.path.join(toolchain_lib_path, 'glib-2.0', 'include'),
    ]
if not is_desktop:
    INCLUDE += [os.path.join(toolchain_sysroot, 'usr', 'include', 'libnm')]
if not is_desktop:
    INCLUDE += [os.path.join(toolchain_sysroot, 'usr', 'include', 'libdrm')]

lvgl_component = list(filter(
    lambda component: component['target'] == 'lvgl_component',
    env['COMPONENTS'],
))[0]
if toolchain_sysroot:
    lvgl_component['INCLUDE'] += [
        os.path.join(toolchain_sysroot, 'usr', 'include', 'freetype2'),
        os.path.join(toolchain_sysroot, 'usr', 'include', 'libpng16'),
    ]
elif is_desktop:
    lvgl_component['INCLUDE'] += desktop_dependency_includes
if not is_desktop:
    lvgl_component['INCLUDE'] += [
        os.path.join(toolchain_sysroot, 'usr', 'include', 'libdrm')
    ]

env['COMPONENTS'].append({
    'target': env['PROJECT_NAME'],
    'SRCS': SRCS,
    'INCLUDE': INCLUDE,
    'PRIVATE_INCLUDE': PRIVATE_INCLUDE,
    'REQUIREMENTS': REQUIREMENTS,
    'STATIC_LIB': STATIC_LIB,
    'DYNAMIC_LIB': DYNAMIC_LIB,
    'DEFINITIONS': DEFINITIONS,
    'DEFINITIONS_PRIVATE': DEFINITIONS_PRIVATE,
    'LDFLAGS': LDFLAGS,
    'LINK_SEARCH_PATH': LINK_SEARCH_PATH,
    'STATIC_FILES': STATIC_FILES,
    'REGISTER': 'project',
})
