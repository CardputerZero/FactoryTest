#ifndef LV_CONF_H
#define LV_CONF_H

#if defined(FACTORY_TEST_SCONS_BUILD)
#include "factory_test_config.h"
#endif

#if USE_DESKTOP
#include "lv_conf_desktop.h"
#else
#include "lv_conf_cm0.h"
#endif

#endif  // LV_CONF_H
