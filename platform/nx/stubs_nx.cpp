// Recursos do PC que o Switch ainda não tem: vídeos remasterizados (ffmpeg) e o aviso de troféu desenhado
// pelo GDI. O jogo segue com os vídeos Bink originais e sem o aviso.
#include "trophy_banner.h"
#include "video_hd.h"

namespace vt {
void video_hd_open(const std::string &, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}
void video_hd_close() {}
bool video_hd_frame(int64_t, const uint8_t *&, uint32_t &, uint32_t &) { return false; }
bool video_hd_active() { return false; }
} // namespace vt

namespace gvk {
bool render_trophy_banner(const TrophyBanner &, uint32_t, BannerImage &) { return false; }
} // namespace gvk
