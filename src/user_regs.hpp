#include <cstdint>
#include <uhd/usrp/multi_usrp.hpp>

#define SR_CORE_PLAY_CTRL_ADDR 0x64
#define SR_CORE_PLAY_CTRL_WIDTH 2

typedef struct {
    uint32_t enable : 1;
    uint32_t trigger_src : 1;
    uint32_t : 30; //reserved
} sr_core_play_ctrl;

void set_sr_core_play(uhd::usrp::multi_usrp::sptr usrp, uint32_t enable,
                      uint32_t trigger_src) {
  union {
    sr_core_play_ctrl play_ctrl;
    uint32_t raw;
  } play_ctrl_un = {.play_ctrl{.enable = enable, .trigger_src = trigger_src}};
  usrp->set_user_register(SR_CORE_PLAY_CTRL_ADDR, play_ctrl_un.raw);
}