/*
 *      Copyright (C) 2011-2013 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAS_IMXVPU
 #include <linux/mxcfb.h>
#endif
#include "system.h"
#include <EGL/egl.h>

#include "Application.h"
#include "EGLNativeTypeIMX.h"
#include <math.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "utils/log.h"
#include "utils/RegExp.h"
#include "utils/StringUtils.h"
#include "utils/Environment.h"
#include "guilib/gui3d.h"
#include "windowing/WindowingFactory.h"
#include "cores/AudioEngine/AEFactory.h"
#include <fstream>
#include "peripherals/Peripherals.h"
#include "peripherals/bus/linux/PeripheralBusPLATFORMLibUdev.h"

using namespace PERIPHERALS;

CEGLNativeTypeIMX::CEGLNativeTypeIMX()
#ifdef HAS_IMXVPU
  : m_sar(0.0f)
  , m_display(NULL)
  , m_window(NULL)
#endif
{
#ifdef HAS_IMXVPU
  m_show = true;
  m_readonly = true;

  g_peripherals.CreatePeripheralBus(new CPeripheralBusPLATFORM(&g_peripherals));
#endif
}

CEGLNativeTypeIMX::~CEGLNativeTypeIMX()
{
}

bool CEGLNativeTypeIMX::CheckCompatibility()
{
#ifdef HAS_IMXVPU
  std::ifstream file("/sys/class/graphics/fb0/fsl_disp_dev_property");
  return file.is_open();
#else
  return false;
#endif
}

void CEGLNativeTypeIMX::Initialize()
{
#ifdef HAS_IMXVPU
  int fd;

  fd = open("/dev/fb0",O_RDWR);
  if (fd < 0)
  {
    CLog::Log(LOGERROR, "%s - Error while opening /dev/fb0.\n", __FUNCTION__);
    return;
  }

  // Check if we can change the framebuffer resolution
  if ((fd = open("/sys/class/graphics/fb0/mode", O_RDWR)) >= 0)
  {
    m_readonly = false;
    close(fd);
    GetNativeResolution(&m_init);
  }

  ShowWindow(false);
#endif
  return;
}

void CEGLNativeTypeIMX::Destroy()
{
#ifdef HAS_IMXVPU
  CLog::Log(LOGDEBUG, "%s\n", __FUNCTION__);
  struct fb_fix_screeninfo fixed_info;
  void *fb_buffer;
  int fd;

  fd = open("/dev/fb0",O_RDWR);
  if (fd < 0)
  {
    CLog::Log(LOGERROR, "%s - Error while opening /dev/fb0.\n", __FUNCTION__);
    return;
  }

  ioctl( fd, FBIOGET_FSCREENINFO, &fixed_info);
  // Black fb0
  fb_buffer = mmap(NULL, fixed_info.smem_len, PROT_WRITE, MAP_SHARED, fd, 0);
  if (fb_buffer == MAP_FAILED)
  {
    CLog::Log(LOGERROR, "%s - fb mmap failed %s.\n", __FUNCTION__, strerror(errno));
  }
  else
  {
    memset(fb_buffer, 0x0, fixed_info.smem_len);
    munmap(fb_buffer, fixed_info.smem_len);
  }
  close(fd);

  if (!m_readonly)
  {
    CLog::Log(LOGDEBUG, "%s changing mode to %s\n", __FUNCTION__, m_init.strId.c_str());
    set_sysfs_str("/sys/class/graphics/fb0/mode", m_init.strId.c_str());
  }

  system("/usr/bin/splash --force -i -m 'stopping xbmc...'");
#endif
  return;
}

bool CEGLNativeTypeIMX::CreateNativeDisplay()
{
  CLog::Log(LOGDEBUG,": %s", __FUNCTION__);
#ifdef HAS_IMXVPU
  if (m_display)
    return true;

  // Force double-buffering
  CEnvironment::setenv("FB_MULTI_BUFFER", "2", 0);
  // EGL will be rendered on fb0
  if (!(m_display = fbGetDisplayByIndex(0)))
    return false;
  m_nativeDisplay = &m_display;
  return true;
#else
  return false;
#endif
}

bool CEGLNativeTypeIMX::CreateNativeWindow()
{
  CLog::Log(LOGDEBUG,": %s", __FUNCTION__);
#ifdef HAS_IMXVPU
  if (m_window)
    return true;

  if (!(m_window = fbCreateWindow(m_display, 0, 0, 0, 0)))
    return false;

  m_nativeWindow = &m_window;
  return true;
#else
  return false;
#endif
}

bool CEGLNativeTypeIMX::GetNativeDisplay(XBNativeDisplayType **nativeDisplay) const
{
#ifdef HAS_IMXVPU
  if (!m_nativeDisplay)
    return false;

  *nativeDisplay = (XBNativeDisplayType*)m_nativeDisplay;
  return true;
#else
  return false;
#endif
}

bool CEGLNativeTypeIMX::GetNativeWindow(XBNativeWindowType **nativeWindow) const
{
#ifdef HAS_IMXVPU
  if (!m_nativeWindow)
    return false;

  *nativeWindow = (XBNativeWindowType*)m_nativeWindow;
  return true;
#else
  return false;
#endif
}

bool CEGLNativeTypeIMX::DestroyNativeDisplay()
{
  CLog::Log(LOGDEBUG,": %s", __FUNCTION__);
#ifdef HAS_IMXVPU
  if (m_display)
    fbDestroyDisplay(m_display);

  m_display = NULL;
  m_nativeDisplay = NULL;
  return true;
#else
  return false;
#endif
}

bool CEGLNativeTypeIMX::DestroyNativeWindow()
{
  CLog::Log(LOGDEBUG,": %s", __FUNCTION__);
#ifdef HAS_IMXVPU
  if (m_window)
    fbDestroyWindow(m_window);

  m_window = NULL;
  m_nativeWindow = NULL;
  return true;
#else
  return false;
#endif
}

bool CEGLNativeTypeIMX::GetNativeResolution(RESOLUTION_INFO *res) const
{
#ifdef HAS_IMXVPU
  std::string mode;
  get_sysfs_str("/sys/class/graphics/fb0/mode", mode);
  CLog::Log(LOGDEBUG,": %s, %s", __FUNCTION__, mode.c_str());

  return ModeToResolution(mode, res);
#else
  return false;
#endif
}

bool CEGLNativeTypeIMX::SetNativeResolution(const RESOLUTION_INFO &res)
{
#ifdef HAS_IMXVPU
  if (m_readonly || !g_application.GetRenderGUI())
    return false;

  std::string mode;
  get_sysfs_str("/sys/class/graphics/fb0/mode", mode);
  if (res.strId == mode)
  {
    CLog::Log(LOGDEBUG,": %s - not changing res (%s vs %s)", __FUNCTION__, res.strId.c_str(), mode.c_str());
    return true;
  }

  DestroyNativeWindow();
  DestroyNativeDisplay();

  ShowWindow(false);
  CLog::Log(LOGDEBUG,": %s - changing resolution to %s", __FUNCTION__, res.strId.c_str());
  set_sysfs_str("/sys/class/graphics/fb0/mode", res.strId.c_str());

  CreateNativeDisplay();
  CreateNativeWindow();

  return true;
#else
  return false;
#endif
}

#ifdef HAS_IMXVPU
bool CEGLNativeTypeIMX::FindMatchingResolution(const RESOLUTION_INFO &res, const std::vector<RESOLUTION_INFO> &resolutions)
{
  for (int i = 0; i < (int)resolutions.size(); i++)
  {
    if(resolutions[i].iScreenWidth == res.iScreenWidth &&
       resolutions[i].iScreenHeight == res.iScreenHeight &&
       resolutions[i].fRefreshRate == res.fRefreshRate &&
      (resolutions[i].dwFlags & D3DPRESENTFLAG_MODEMASK) == (res.dwFlags & D3DPRESENTFLAG_MODEMASK))
    {
       return true;
    }
  }
  return false;
}
#endif

bool CEGLNativeTypeIMX::ProbeResolutions(std::vector<RESOLUTION_INFO> &resolutions)
{
#ifdef HAS_IMXVPU
  GetMonitorSAR();

  if (m_readonly)
    return false;

  std::string valstr;
  get_sysfs_str("/sys/class/graphics/fb0/modes", valstr);
  std::vector<std::string> probe_str = StringUtils::Split(valstr, "\n");

  // lexical order puts the modes list into our preferred
  // order and by later filtering through FindMatchingResolution()
  // we make sure we read _all_ S modes, following U and V modes
  // while list will hold unique resolutions only
  std::sort(probe_str.begin(), probe_str.end());

  resolutions.clear();
  RESOLUTION_INFO res;
  for (size_t i = 0; i < probe_str.size(); i++)
  {
    if(!StringUtils::StartsWithNoCase(probe_str[i], "S:") && !StringUtils::StartsWithNoCase(probe_str[i], "U:") &&
       !StringUtils::StartsWithNoCase(probe_str[i], "V:") && !StringUtils::StartsWithNoCase(probe_str[i], "D:") &&
       !StringUtils::StartsWithNoCase(probe_str[i], "H:") && !StringUtils::StartsWithNoCase(probe_str[i], "T:"))
      continue;

    if(ModeToResolution(probe_str[i], &res))
      if(!FindMatchingResolution(res, resolutions))
        resolutions.push_back(res);
  }
  return resolutions.size() > 0;
#else
  return false;
#endif
}

bool CEGLNativeTypeIMX::GetPreferredResolution(RESOLUTION_INFO *res) const
{
#ifdef HAS_IMXVPU
  return GetNativeResolution(res);
#else
  return false;
#endif
}

bool CEGLNativeTypeIMX::ShowWindow(bool show)
{
#ifdef HAS_IMXVPU
  if (m_show == show)
    return true;

  CLog::Log(LOGDEBUG, ": %s %s", __FUNCTION__, show?"show":"hide");
  set_sysfs_str("/sys/class/graphics/fb0/blank", show?"0":"1");

  m_show = show;
  return true;
#else
  return false;
#endif
}

#ifdef HAS_IMXVPU
float CEGLNativeTypeIMX::ValidateSAR(struct dt_dim *dtm, bool mb)
{
  int Height = dtm->Height | (mb ? (dtm->msbits & 0x0f) << 8 : 0);
  if (Height < 1)
    return .0f;

  int Width = dtm->Width | (mb ? (dtm->msbits & 0xf0) << 4 : 0);
  float t_sar = (float) Width / Height;

  if (t_sar < 0.33 || t_sar > 3.00)
    t_sar = .0f;
  else
    CLog::Log(LOGDEBUG, "%s: Screen SAR: %.3f (from detailed: %s, %dx%d)",__FUNCTION__, t_sar, mb ? "yes" : "no", Width, Height);

  return t_sar;
}

void CEGLNativeTypeIMX::GetMonitorSAR()
{
  FILE *f_edid;
  char *str = NULL;
  unsigned char p;
  size_t n;
  int done = 0;

  // kernels <= 3.18 use ./soc0/soc.1 in official imx kernel
  // kernels  > 3.18 use ./soc0/soc
  m_sar = 0;
  f_edid = fopen("/sys/devices/soc0/soc/20e0000.hdmi_video/edid", "r");
  if(!f_edid)
    f_edid = fopen("/sys/devices/soc0/soc.1/20e0000.hdmi_video/edid", "r");

  if(!f_edid)
    return;

  // we need to convert mxc_hdmi output format to binary array
  // mxc_hdmi provides the EDID as space delimited 1bytes blocks
  // exported as text with format specifier %x eg:
  // 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x4C 0x2D 0x7A 0x0A 0x00 0x00 0x00 0x00
  //
  // this translates into the inner cycle where we move pointer first
  // with +2 to skip '0x',
  // we sscanf actual data (eg FF) into a byte,
  // we move over the FF and delimiting space with +3
  //
  // this parses whole 512 byte long info into internal binary array for future
  // reference and use. current use is only to grab screen's physical params
  // at EGL init.
  while(getline(&str, &n, f_edid) > 0)
  {
    char *c = str;
    while(*c != '\n' && done < 512)
    {
      c += 2;
      sscanf(c, "%hhx", &p);
      m_edid[done++] = p;
      c += 3;
    }
    if (str)
      free(str);
    str = NULL;
  }
  fclose(f_edid);

  // enumerate through (max four) detailed timing info blocks
  // specs and lookup WxH [mm / in]. W and H are in 3 bytes,
  // where 1st = W, 2nd = H, 3rd byte is 4bit/4bit.
  for (int i = EDID_DTM_START; i < 126 && m_sar == 0; i += 18)
    m_sar = ValidateSAR((struct dt_dim *)(m_edid +i +EDID_DTM_OFFSET_DIMENSION), true);

  // fallback - info related to 'Basic display parameters.' is at offset 0x14-0x18.
  // where W is 2nd byte, H 3rd.
  if (m_sar == 0)
    m_sar = ValidateSAR((struct dt_dim *)(m_edid +EDID_STRUCT_DISPLAY +1));

  // if m_sar != 0, final SAR is usefull
  // if it is 0, EDID info was missing or calculated
  // SAR value wasn't sane
  if (m_sar == 0)
    CLog::Log(LOGDEBUG, "%s: Screen SAR - not usable info",__FUNCTION__);
}

bool CEGLNativeTypeIMX::get_sysfs_str(std::string path, std::string& valstr) const
{
  int len;
  char buf[256] = {0};

  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0)
  {
    CLog::Log(LOGERROR, "%s: error reading %s",__FUNCTION__, path.c_str());
    valstr = "fail";
    return false;
  }

  while ((len = read(fd, buf, 255)) > 0)
    valstr.append(buf, len);

  StringUtils::Trim(valstr);
  close(fd);

  return true;
}

bool CEGLNativeTypeIMX::set_sysfs_str(std::string path, std::string val) const
{
  int fd = open(path.c_str(), O_WRONLY);
  if (fd < 0)
  {
    CLog::Log(LOGERROR, "%s: error writing %s",__FUNCTION__, path.c_str());
    return false;
  }

  val += '\n';
  write(fd, val.c_str(), val.size());
  close(fd);

  return true;
}

bool CEGLNativeTypeIMX::ModeToResolution(std::string mode, RESOLUTION_INFO *res) const
{
  if (!res)
    return false;

  res->iWidth = 0;
  res->iHeight= 0;

  if(mode.empty())
    return false;

  std::string fromMode = StringUtils::Mid(mode, 2);
  StringUtils::Trim(fromMode);

  res->dwFlags = 0;
  res->fPixelRatio = 1.0f;

  if (StringUtils::StartsWith(mode, "H:")) {
    res->dwFlags |= D3DPRESENTFLAG_MODE3DSBS;
    res->fPixelRatio = 2.0f;
  } else if (StringUtils::StartsWith(mode, "T:")) {
    res->dwFlags |= D3DPRESENTFLAG_MODE3DTB;
    res->fPixelRatio = 0.5f;
  } else if (StringUtils::StartsWith(mode, "F:")) {
    return false;
  }

  CRegExp split(true);
  split.RegComp("([0-9]+)x([0-9]+)([pi])-([0-9]+)");
  if (split.RegFind(fromMode) < 0)
    return false;

  int w = atoi(split.GetMatch(1).c_str());
  int h = atoi(split.GetMatch(2).c_str());
  std::string p = split.GetMatch(3);
  int r = atoi(split.GetMatch(4).c_str());

  res->iWidth = w;
  res->iHeight= h;
  res->iScreenWidth = w;
  res->iScreenHeight= h;
  res->fRefreshRate = (float)r;
  if (StringUtils::isasciilowercaseletter(mode[0]))
    res->fRefreshRate *= ((float)1000 / 1001);
  res->dwFlags |= p[0] == 'p' ? D3DPRESENTFLAG_PROGRESSIVE : D3DPRESENTFLAG_INTERLACED;

  res->iScreen       = 0;
  res->bFullScreen   = true;
  res->iSubtitles    = (int)(0.965 * res->iHeight);
  res->fPixelRatio  *= !m_sar ? 1.0f : (float)m_sar / res->iScreenWidth * res->iScreenHeight;
  res->strMode       = StringUtils::Format("%4sx%4s @ %.3f%s - Full Screen (%.3f) %s", StringUtils::Format("%d", res->iScreenWidth).c_str(),
                                           StringUtils::Format("%d", res->iScreenHeight).c_str(), res->fRefreshRate,
                                           res->dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : " ", res->fPixelRatio,
                                           res->dwFlags & D3DPRESENTFLAG_MODE3DSBS ? "- 3DSBS" : res->dwFlags & D3DPRESENTFLAG_MODE3DTB ? "- 3DTB" : "");
  res->strId         = mode;

  return res->iWidth > 0 && res->iHeight> 0;
}
#endif
