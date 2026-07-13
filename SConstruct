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


KCONFIG_REQUIREMENT_PREFIX = 'CONFIG_FACTORY_TEST_USE_'
feature_enabled = {}
kconfig_feature_macros = {}
kconfig_requirements = []
factory_test_config_names = set(os.environ)
global_config_mk = os.path.join(
    env['PROJECT_PATH'], 'build', 'config', 'global_config.mk'
)
with open(global_config_mk, encoding='utf-8') as config_file:
    for config_line in config_file:
        normalized_line = config_line.lstrip('# ').strip()
        if normalized_line.startswith(KCONFIG_REQUIREMENT_PREFIX):
            factory_test_config_names.add(
                normalized_line.split('=', 1)[0].split(' is not set', 1)[0]
            )

for config_name in factory_test_config_names:
    if not config_name.startswith(KCONFIG_REQUIREMENT_PREFIX):
        continue
    encoded_requirements = config_name[len(KCONFIG_REQUIREMENT_PREFIX):]
    if '_FGF_' not in encoded_requirements:
        continue
    config_parts = encoded_requirements.split('_FGF_')
    if len(config_parts) < 3:
        continue
    macro_name, feature_name = config_parts[:2]
    requirement_names = config_parts[2:]
    is_enabled = config_enabled(config_name)
    kconfig_feature_macros[macro_name] = is_enabled
    feature_enabled[feature_name.lower()] = is_enabled
    if not is_enabled:
        continue
    kconfig_requirements += [
        requirement_name.replace('_GANG_', '-').replace('_DOT_', '.')
        for requirement_name in requirement_names
    ]


feature_enabled.update({
    'desktop': config_enabled('CONFIG_FACTORY_TEST_USE_DESKTOP'),
    'drm': config_enabled('CONFIG_FACTORY_TEST_USE_DRM'),
    'bluez': config_enabled('CONFIG_FACTORY_TEST_USE_BLUEZ'),
    'miniaudio': config_enabled('CONFIG_FACTORY_TEST_USE_MINIAUDIO'),
})

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
] + kconfig_requirements
if toolchain_sysroot:
    REQUIREMENTS += [':libfmt.so.10']
if feature_enabled['desktop']:
    REQUIREMENTS += pkg_config_ldflags('sdl2')
if feature_enabled['drm']:
    REQUIREMENTS += ['drm']
if feature_enabled['bluez']:
    REQUIREMENTS += ['gio-2.0', 'gobject-2.0', 'glib-2.0']
REQUIREMENTS = list(dict.fromkeys(REQUIREMENTS))
STATIC_LIB = []
DYNAMIC_LIB = []

desktop_dependency_includes = []
if feature_enabled['desktop']:
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
factory_test_config_lines = ['#pragma once\n\n', '#define APP_SCONS_CONFIG 1\n']
config_macro_values = {
    'USE_DESKTOP': feature_enabled['desktop'],
    'APP_USE_DRM': feature_enabled['drm'],
    'APP_USE_BLUEZ': feature_enabled['bluez'],
    'APP_USE_MINIAUDIO': feature_enabled['miniaudio'],
}
config_macro_values.update(kconfig_feature_macros)
for macro_name, macro_enabled in config_macro_values.items():
    factory_test_config_lines.append(
        '#define {} {}\n'.format(
            macro_name,
            1 if macro_enabled else 0,
        )
    )
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
if feature_enabled['bluez'] or feature_enabled.get('libnm', False):
    INCLUDE += [
        os.path.join(toolchain_sysroot, 'usr', 'include', 'glib-2.0'),
        os.path.join(toolchain_lib_path, 'glib-2.0', 'include'),
    ]
if feature_enabled.get('libnm', False):
    INCLUDE += [os.path.join(toolchain_sysroot, 'usr', 'include', 'libnm')]
if feature_enabled['drm']:
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
elif feature_enabled['desktop']:
    lvgl_component['INCLUDE'] += desktop_dependency_includes
if feature_enabled['drm']:
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
