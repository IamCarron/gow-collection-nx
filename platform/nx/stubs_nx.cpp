// Recurso do PC que o Switch ainda não tem: o aviso de troféu desenhado pelo GDI. O jogo segue sem o aviso.
// (Os vídeos remasterizados agora estão em video_hd_nx.cpp, pelo NVDEC.)
#include "trophy_banner.h"

namespace gvk {
bool render_trophy_banner(const TrophyBanner &, uint32_t, BannerImage &) { return false; }
} // namespace gvk
