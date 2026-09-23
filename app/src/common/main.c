/*******************************************************************************
 *   (c) 2016 Ledger
 *   (c) 2018, 2019 Zondax GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ********************************************************************************/
#include "app_main.h"
#include "view.h"

#include <os_io_seproxyhal.h>

#ifdef HAVE_NBGL
#include "nbgl_use_case.h"
#endif

// Helper to quit the application in a limited THROW context
static void app_exit(void) {
  BEGIN_TRY_L(exit) {
    TRY_L(exit) { os_sched_exit(-1); }
    FINALLY_L(exit) {}
  }
  END_TRY_L(exit);
}

__attribute__((section(".boot"))) int main(int arg0) {
  // exit critical section
  __asm volatile("cpsie i");

  os_boot();

  if (arg0 != 0) {
    // Library mode is not supported: no app is registered to call this one.
    app_exit();
  } else {
    BEGIN_TRY {
      TRY {
        view_init();
        app_init();
        app_main();
      }
      CATCH_OTHER(e) { UNUSED(e); }
      FINALLY {}
    }
    END_TRY;
  }
}
