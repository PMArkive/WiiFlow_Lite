
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <algorithm>
#include <new>
#include <zlib.h>
#include <cwctype>
#include <sys/stat.h>
#include "coverflow.hpp"
#include "pngu.h"
#include "boxmesh.hpp"
#include "lockMutex.hpp"
#include "fonts.h"
#include "types.h"
#include "gui/fmt.h"
#include "gecko/gecko.hpp"
#include "menu/menu.hpp"
#include "plugin/plugin.hpp"
#include "memory/mem2.hpp"
#include "fileOps/fileOps.h"
#include "wstringEx/wstringEx.hpp"

static const int black_len = 16;
static const char* black[black_len] = {"RZZJEL","RZNJ01","SEKJ99","SX3J01","SX3P01","R5WJA4","RUYJ99","S3HJ08","SJBJ01","CKBE88","CCPE01","SMMP01","MDUE01","SL2J01","APR","AFR"};

static const int red_len = 2;
static const char* red[red_len] = {"SMN","HBW"};

static const int yellow_len = 2;
static const char* yellow[yellow_len] = {"SIIE8P","SIIP8P"};

static const int greenOne_len = 1;
static const char* greenOne[greenOne_len] = {"SF8J01"};

static const int greenTwo_len = 1;
static const char* greenTwo[greenTwo_len] = {"PDUE01"};

static lwp_t coverLoaderThread = LWP_THREAD_NULL;

static inline int loopNum(int i, int s)
{
	return i < 0 ? (s - (-i % s)) % s : i % s;
}

CCoverFlow CoverFlow;

u8 CCoverFlow::coverThreadStack[32768] ATTRIBUTE_ALIGN(32);
const u32 CCoverFlow::coverThreadStackSize = 32768;

CCoverFlow::CCover::CCover(void)
{
	index = 0;
	txtAngle = 0.f;
	txtTargetAngle = 0.f;
	txtColor = 0;
	txtTargetColor = 0;
	color = 0x00FFFFFF;
	targetColor = 0xFFFFFFFF;
	shadowColor = 0x00000000;
	targetShadowColor = 0x00000000;
	scale = Vector3D(1.f, 1.f, 1.f);
	targetScale = Vector3D(1.f, 1.f, 1.f);
}

CCoverFlow::CItem::CItem(dir_discHdr *itemHdr, int playcount, unsigned int lastPlayed) :
	hdr(itemHdr),
	playcount(playcount),
	lastPlayed(lastPlayed),
	boxTexture(false),
	state(STATE_Loading)
{

}

static inline wchar_t upperCaseWChar(wchar_t c)
{
	return c >= L'a' && c <= L'z' ? c & 0x00DF : c;
}

float m_normal_speed;
float m_selected_speed;

CCoverFlow::CCoverFlow(void)
{
	m_loNormal.camera = Vector3D(0.f, 1.5f, 5.f);
	m_loNormal.cameraAim = Vector3D(0.f, 0.f, -1.f);
	m_loNormal.leftScale = Vector3D(1.f, 1.f, 1.f);
	m_loNormal.rightScale = Vector3D(1.f, 1.f, 1.f);
	m_loNormal.centerScale = Vector3D(1.f, 1.f, 1.f);
	m_loNormal.rowCenterScale = Vector3D(1.f, 1.f, 1.f);
	m_loNormal.leftPos = Vector3D(-1.6f, 0.f, 0.f);
	m_loNormal.rightPos = Vector3D(1.6f, 0.f, 0.f);
	m_loNormal.centerPos = Vector3D(0.f, 0.f, 1.f);
	m_loNormal.rowCenterPos = Vector3D(0.f, 0.f, 0.f);
	m_loNormal.leftAngle = Vector3D(0.f, 70.f, 0.f);
	m_loNormal.rightAngle = Vector3D(0.f, -70.f, 0.f);
	m_loNormal.centerAngle = Vector3D(0.f, 0.f, 0.f);
	m_loNormal.rowCenterAngle = Vector3D(0.f, 0.f, 0.f);
	m_loNormal.leftSpacer = Vector3D(-0.35f, 0.f, 0.f);
	m_loNormal.rightSpacer = Vector3D(0.35f, 0.f, 0.f);
	m_loNormal.leftDeltaAngle = Vector3D(0.f, 0.f, 0.f);
	m_loNormal.rightDeltaAngle = Vector3D(0.f, 0.f, 0.f);
	m_loNormal.txtLeftAngle = -55.f;
	m_loNormal.txtRightAngle = 55.f;
	m_loNormal.txtCenterAngle = 0.f;
	m_loNormal.txtLeftPos = Vector3D(-4.f, 0.f, 1.3f);
	m_loNormal.txtRightPos = Vector3D(4.f, 0.f, 1.3f);
	m_loNormal.txtCenterPos = Vector3D(0.f, 0.f, 2.6f);
	m_loNormal.txtSideWidth = 500.f;
	m_loNormal.txtCenterWidth = 500.f;
	m_loNormal.txtSideStyle = FTGX_ALIGN_BOTTOM | FTGX_JUSTIFY_CENTER;
	m_loNormal.txtCenterStyle = FTGX_ALIGN_BOTTOM | FTGX_JUSTIFY_CENTER;
	m_loNormal.endColor = CColor(0x3FFFFFFF);
	m_loNormal.begColor = CColor(0xCFFFFFFF);
	m_loNormal.mouseOffColor = CColor(0xFF00FF00);
	m_loNormal.shadowColorCenter = CColor(0x00000000);
	m_loNormal.shadowColorEnd = CColor(0x00000000);
	m_loNormal.shadowColorBeg = CColor(0x00000000);
	m_loNormal.shadowColorOff = CColor(0x00000000);
	m_loNormal.topSpacer = Vector3D(0.f, 2.f, 0.f);
	m_loNormal.bottomSpacer = Vector3D(0.f, -2.f, 0.f);
	m_loNormal.topDeltaAngle = Vector3D(0.f, 0.f, 0.f);
	m_loNormal.bottomDeltaAngle = Vector3D(0.f, 0.f, 0.f);
	m_loNormal.topAngle = Vector3D(0.f, 0.f, 0.f);
	m_loNormal.bottomAngle = Vector3D(0.f, 0.f, 0.f);
	// 
	m_loSelected.camera = Vector3D(0.f, 1.5f, 5.f);
	m_loSelected.cameraAim = Vector3D(0.f, 0.f, -1.f);
	m_loSelected.leftScale = Vector3D(1.f, 1.f, 1.f);
	m_loSelected.rightScale = Vector3D(1.f, 1.f, 1.f);
	m_loSelected.centerScale = Vector3D(1.f, 1.f, 1.f);
	m_loSelected.rowCenterScale = Vector3D(1.f, 1.f, 1.f);
	m_loSelected.leftPos = Vector3D(-4.6f, 2.f, 0.f);
	m_loSelected.rightPos = Vector3D(4.6f, 2.f, 0.f);
	m_loSelected.centerPos = Vector3D(-0.6f, 0.f, 2.6f);
	m_loSelected.rowCenterPos = Vector3D(0.f, 0.f, 0.f);
	m_loSelected.leftAngle = Vector3D(-45.f, 90.f, 0.f);
	m_loSelected.rightAngle = Vector3D(-45.f, -90.f, 0.f);
	m_loSelected.centerAngle = Vector3D(0.f, 380.f, 0.f);
	m_loSelected.rowCenterAngle = Vector3D(0.f, 0.f, 0.f);
	m_loSelected.leftSpacer = Vector3D(-0.35f, 0.f, 0.f);
	m_loSelected.rightSpacer = Vector3D(0.35f, 0.f, 0.f);
	m_loSelected.leftDeltaAngle = Vector3D(0.f, 0.f, 0.f);
	m_loSelected.rightDeltaAngle = Vector3D(0.f, 0.f, 0.f);
	m_loSelected.txtLeftAngle = -55.f;
	m_loSelected.txtRightAngle = 55.f;
	m_loSelected.txtCenterAngle = 0.f;
	m_loSelected.txtLeftPos = Vector3D(-4.35f, 0.f, 1.3f);
	m_loSelected.txtRightPos = Vector3D(4.35f, 0.f, 1.3f);
	m_loSelected.txtCenterPos = Vector3D(2.3f, 1.8f, 1.f);
	m_loSelected.txtSideWidth = 500.f;
	m_loSelected.txtCenterWidth =  310.f;
	m_loSelected.txtSideStyle = FTGX_ALIGN_BOTTOM | FTGX_JUSTIFY_CENTER;
	m_loSelected.txtCenterStyle = FTGX_ALIGN_TOP | FTGX_JUSTIFY_RIGHT;
	m_loSelected.endColor = CColor(0x3FFFFFFF);
	m_loSelected.begColor = CColor(0xCFFFFFFF);
	m_loSelected.mouseOffColor = CColor(0xFF00FF00);
	m_loSelected.shadowColorCenter = CColor(0x00000000);
	m_loSelected.shadowColorEnd = CColor(0x00000000);
	m_loSelected.shadowColorBeg = CColor(0x00000000);
	m_loSelected.shadowColorOff = CColor(0x00000000);
	m_loSelected.topSpacer = Vector3D(0.f, 2.f, 0.f);
	m_loSelected.bottomSpacer = Vector3D(0.f, -2.f, 0.f);
	m_loSelected.topDeltaAngle = Vector3D(0.f, 0.f, 0.f);
	m_loSelected.bottomDeltaAngle = Vector3D(0.f, 0.f, 0.f);
	m_loSelected.topAngle = Vector3D(0.f, 0.f, 0.f);
	m_loSelected.bottomAngle = Vector3D(0.f, 0.f, 0.f);
	// 
	m_mirrorAlpha = 0.2f;
	m_txtMirrorAlpha = 0.2f;
	m_delay = 0;
	m_minDelay = 5;
	m_jump = 0;
	m_mutex = 0;
	m_dvdskin_loaded = false;
	m_defcovers_loaded = false;
	m_loadingCovers = false;
	m_coverThrdBusy = false;
	m_renderTex = false;
	m_renderingTex = NULL;
	m_moved = false;
	m_selected = false;
	m_hideCover = false;
	m_tickCount = 0;
	m_blurRadius = 3;
	m_blurFactor = 1.f;
	// 
	m_mirrorBlur = false;
	m_effectTex.width = 96;
	m_effectTex.height = 72;
	m_effectTex.format = GX_TF_RGBA8;
	m_shadowScale = 1.f;
	m_shadowX = 0.f;
	m_shadowY = 0.f;
	m_flipCoverPos = Vector3D(0.f, 0.f, 0.f);
	m_flipCoverAngle = Vector3D(0.f, 180.f, 0.f);
	m_flipCoverScale = Vector3D(1.f, 1.f, 1.f);
	// Settings
	m_lodBias = -0.3f;
	m_aniso = GX_ANISO_1;
	m_edgeLOD = false;
	m_numBufCovers = 20;
	m_compressTextures = true;
	m_compressCache = false;
	m_deletePicsAfterCaching = false;
	m_box = true;
	m_smallBox = false;
	m_useHQcover = false;
	m_rows = 1;
	m_columns = 11;
	m_range = m_rows * m_columns;
	for(int chan = WPAD_MAX_WIIMOTES-1; chan >= 0; chan--)
		m_mouse[chan] = -1;
	sndCopyNum = 0;
	m_soundVolume = 0xFF;
	m_sorting = SORT_ALPHA;
	m_normal_speed = 0.1f;
	m_selected_speed = 0.07f;
	// 
	m_flipSound = NULL;
	m_hoverSound = NULL;
	m_selectSound = NULL;
	m_cancelSound = NULL;
	//
	m_covers = NULL;
	LWP_MutexInit(&m_mutex, 0);
}

bool CCoverFlow::init(const u8 *font, const u32 font_size, bool vid_50hz)
{
	// Load font
	m_font.fromBuffer(font, font_size, TITLEFONT);
	m_fontColor = CColor(0xFFFFFFFF);

	if(vid_50hz)
	{
		gprintf("WiiFlow is in 50hz mode\n");
		m_normal_speed = 0.12f;
		m_selected_speed = 0.084f;
		m_minDelay = 4;
	}

	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9)
		guPerspective(m_projMtx, 45, 16.f / 9.f, .1f, 300.f);
	else
		guPerspective(m_projMtx, RadToDeg(2.f * atan(tan(DegToRad(45 / 2)) / (4.f / 3.f) * (16.f / 9.f))), 4.f / 3.f, .1f, 300.f);
	return true;
}

void CCoverFlow::simulateOtherScreenFormat(bool s)
{
	if ((CONF_GetAspectRatio() == CONF_ASPECT_16_9) != s)
		guPerspective(m_projMtx, 45, 16.f / 9.f, .1f, 300.f);
	else
		guPerspective(m_projMtx, RadToDeg(2.f * atan(tan(DegToRad(45 / 2)) / (4.f / 3.f) * (16.f / 9.f))), 4.f / 3.f, .1f, 300.f);
}

CCoverFlow::~CCoverFlow(void)
{
	shutdown();
	LWP_MutexDestroy(m_mutex);
}

void CCoverFlow::setCachePath(const char *path)
{
	m_cachePath = path;
}

void CCoverFlow::setTextureQuality(float lodBias, int aniso, bool edgeLOD)
{
	m_lodBias = min(max(-3.f, lodBias), 1.f);
	switch (aniso)
	{
		case 1:
			m_aniso = GX_ANISO_2;
			break;
		case 2:
			m_aniso = GX_ANISO_4;
			break;
		default:
			m_aniso = GX_ANISO_1;
	}
	m_edgeLOD = edgeLOD;
}

void CCoverFlow::setBoxMode(bool box)
{
	m_box = box;
}

void CCoverFlow::setSmallBoxMode(bool smallBox)
{
	m_smallBox = smallBox;
}

void CCoverFlow::setHQcover(bool HQ)
{
	m_useHQcover = HQ;
}

void CCoverFlow::setBufferSize(u32 numCovers)
{
	if(m_covers != NULL) return;
	m_numBufCovers = min(max(4ul, numCovers / 2ul), 40ul);//8 to 80 
}

void CCoverFlow::setTextures(const string &loadingPic, const string &loadingPicFlat, const string &noCoverPic, const string &noCoverPicFlat)
{
	if (m_covers != NULL) return;
	m_pngLoadCover = loadingPic;
	m_pngLoadCoverFlat = loadingPicFlat;
	m_pngNoCover = noCoverPic;
	m_pngNoCoverFlat = noCoverPicFlat;
}

void CCoverFlow::setFont(const SFont &font, const CColor &color)
{
	if(m_font.font != NULL)
		delete m_font.font;
	m_font = font;
	m_fontColor = color;
	if (m_covers != NULL)
	{
		for (u32 i = 0; i < m_range; ++i)
			_loadCover(i, m_covers[i].index);
		_updateAllTargets();
	}
}

void CCoverFlow::_transposeCover(CCover* &dst, u32 rows, u32 columns, int pos)
{
	int i = pos - (int)(rows * columns / 2);
	int j = rows >= 3 ? abs(i) - ((abs(i) + (int)rows / 2) / (int)rows) * 2 : abs(i);
	if (m_rows >= 3)
		j += ((j + ((int)m_rows - 2) / 2) / ((int)m_rows - 2)) * 2;
	int k = m_range / 2 + (i < 0 ? -j : j);
	if ((u32)k < m_range)
		dst[pos] = m_covers[k];
}

void CCoverFlow::setRange(u32 rows, u32 columns)
{
	rows = rows < 3ul ? 1ul : min(rows | 1ul, 9ul) + 2ul;
	columns = min(max(3ul, columns | 1ul), 21ul) + 2ul;
	u32 range = rows * columns;
	if (rows == m_rows && columns == m_columns && range == m_range)
		return;
	if (m_covers != NULL)
	{
		stopCoverLoader();
		MEM2_free(m_covers);
		CCover *tmpCovers = (CCover*)MEM2_alloc(sizeof(CCover) * range);
		for(size_t i = 0; i < range; ++i)
		{
			// does not allocate memory -- calls: operator new (sizeof(CCover), tmpCovers+i)
			// only constructs an object at tmpCovers+i
			// delete is not needed
			// this is the same as CCover tmpCovers[range] except the objects memory address is specified and won't go out of scope
			tmpCovers[i] = *(new (tmpCovers+i) CCover);
		}
		if (rows >= 3)
			for (u32 x = 0; x < columns; ++x)
				for (u32 y = 1; y < rows - 1; ++y)
					_transposeCover(tmpCovers, rows, columns, x * rows + y);
		else
			for (u32 x = 0; x < range; ++x)
				_transposeCover(tmpCovers, rows, columns, x);
		m_rows = rows;
		m_columns = columns;
		m_range = range;
		m_covers = tmpCovers;
		_loadAllCovers(m_covers[m_range / 2].index);
		_updateAllTargets();
		startCoverLoader();
	}
	else
	{
		m_rows = rows;
		m_columns = columns;
		m_range = range;
	}
}

void CCoverFlow::setCameraPos(bool selected, const Vector3D &pos, const Vector3D &aim)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;
	lo.camera = pos;
	lo.cameraAim = aim;
}

void CCoverFlow::setCameraOsc(bool selected, const Vector3D &speed, const Vector3D &amp)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;
	lo.cameraOscSpeed = speed;
	lo.cameraOscAmp = amp;
}

void CCoverFlow::setCoverScale(bool selected, const Vector3D &left, const Vector3D &right, const Vector3D &center, const Vector3D &rowCenter)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;
	lo.leftScale = left;
	lo.rightScale = right;
	lo.centerScale = center;
	lo.rowCenterScale = rowCenter;
}

void CCoverFlow::setCoverPos(bool selected, const Vector3D &left, const Vector3D &right, const Vector3D &center, const Vector3D &rowCenter)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;
	lo.leftPos = left;
	lo.rightPos = right;
	lo.centerPos = center;
	lo.rowCenterPos = rowCenter;
}

void CCoverFlow::setCoverAngleOsc(bool selected, const Vector3D &speed, const Vector3D &amp)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;
	lo.coverOscASpeed = speed;
	lo.coverOscAAmp = amp;
}

void CCoverFlow::setCoverPosOsc(bool selected, const Vector3D &speed, const Vector3D &amp)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;
	lo.coverOscPSpeed = speed;
	lo.coverOscPAmp = amp;
}

void CCoverFlow::setSpacers(bool selected, const Vector3D &left, const Vector3D &right)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;
	lo.leftSpacer = left;
	lo.rightSpacer = right;
}

void CCoverFlow::setDeltaAngles(bool selected, const Vector3D &left, const Vector3D &right)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;
	lo.leftDeltaAngle = left;
	lo.rightDeltaAngle = right;
}

void CCoverFlow::setAngles(bool selected, const Vector3D &left, const Vector3D &right, const Vector3D &center, const Vector3D &rowCenter)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;
	lo.leftAngle = left;
	lo.rightAngle = right;
	lo.centerAngle = center;
	lo.rowCenterAngle = rowCenter;
}

void CCoverFlow::setTitleAngles(bool selected, float left, float right, float center)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;
	lo.txtLeftAngle = left;
	lo.txtRightAngle = right;
	lo.txtCenterAngle = center;
}

void CCoverFlow::setTitlePos(bool selected, const Vector3D &left, const Vector3D &right, const Vector3D &center)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;
	lo.txtLeftPos = left;
	lo.txtRightPos= right;
	lo.txtCenterPos= center;
}

void CCoverFlow::setTitleWidth(bool selected, float side, float center)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;
	lo.txtSideWidth = side;
	lo.txtCenterWidth= center;
}

void CCoverFlow::setTitleStyle(bool selected, u16 side, u16 center)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;
	lo.txtSideStyle = side;
	lo.txtCenterStyle = center;
}

void CCoverFlow::setColors(bool selected, const CColor &begColor, const CColor &endColor, const CColor &offColor)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;

	lo.begColor = begColor;
	lo.endColor = endColor;
	lo.mouseOffColor = offColor;
}

void CCoverFlow::setMirrorAlpha(float cover, float title)
{
	m_mirrorAlpha = cover;
	m_txtMirrorAlpha = title;
}

void CCoverFlow::setMirrorBlur(bool blur)
{
	m_mirrorBlur = blur;
}

void CCoverFlow::setShadowColors(bool selected, const CColor &centerColor, const CColor &begColor, const CColor &endColor, const CColor &offColor)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;

	lo.shadowColorCenter = centerColor;
	lo.shadowColorBeg = begColor;
	lo.shadowColorEnd = endColor;
	lo.shadowColorOff = offColor;
}

void CCoverFlow::setShadowPos(float scale, float x, float y)
{
	m_shadowScale = scale;
	m_shadowX = x;
	m_shadowY = y;
}

void CCoverFlow::setRowSpacers(bool selected, const Vector3D &top, const Vector3D &bottom)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;

	lo.topSpacer = top;
	lo.bottomSpacer = bottom;
}

void CCoverFlow::setRowDeltaAngles(bool selected, const Vector3D &top, const Vector3D &bottom)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;

	lo.topDeltaAngle = top;
	lo.bottomDeltaAngle = bottom;
}

void CCoverFlow::setRowAngles(bool selected, const Vector3D &top, const Vector3D &bottom)
{
	SLayout &lo = selected ? m_loSelected : m_loNormal;

	lo.topAngle = top;
	lo.bottomAngle = bottom;
}

void CCoverFlow::setCoverFlipping(const Vector3D &pos, const Vector3D &angle, const Vector3D &scale)
{
	m_flipCoverPos = pos;
	m_flipCoverAngle = angle;
	m_flipCoverScale = scale;
}

void CCoverFlow::setCoverFlipPos(const Vector3D &pos)
{
	if(m_covers == NULL || !m_selected) return;
	LockMutex lock(m_mutex);
	m_covers[m_range / 2].targetPos = m_loSelected.centerPos + pos;
}

void CCoverFlow::setBlur(u32 blurResolution, u32 blurRadius, float blurFactor)
{
	static const struct { u32 x; u32 y; } blurRes[] = {
		{ 64, 48 }, { 96, 72 }, { 128, 96 }, { 192, 144 }
	};
	u32 i = min(max(0u, blurResolution), sizeof blurRes / sizeof blurRes[0] - 1u);
	m_effectTex.width = blurRes[i].x;
	m_effectTex.height = blurRes[i].y;
	if(m_effectTex.data != NULL)
	{
		free(m_effectTex.data);
		m_effectTex.data = NULL;
	}
	m_blurRadius = min(max(1u, blurRadius), 3u);
	m_blurFactor = min(max(1.f, blurFactor), 2.f);
}

void CCoverFlow::setSorting(Sorting sorting)
{
	m_sorting = sorting;
}

void CCoverFlow::setSounds(GuiSound *flipSound, GuiSound *hoverSound, GuiSound *selectSound, GuiSound *cancelSound)
{
	//for(u8 i = 0; i < 4; i++)
	m_flipSound = flipSound;
	m_hoverSound = hoverSound;
	m_selectSound = selectSound;
	m_cancelSound = cancelSound;
}

void CCoverFlow::setSoundVolume(u8 vol)
{
	m_soundVolume = vol;
}

void CCoverFlow::_stopSound(GuiSound * &snd)
{
	if(snd == NULL) return;
	snd->Stop();
}

void CCoverFlow::_playSound(GuiSound * &snd)
{
	if(snd == NULL) return;
	snd->Play(m_soundVolume);
}

void CCoverFlow::stopSound(void)
{
	//for(u8 i = 0; i < 4; i++)
	_stopSound(m_flipSound);
	_stopSound(m_hoverSound);
	_stopSound(m_selectSound);
	_stopSound(m_cancelSound);
}

void CCoverFlow::applySettings(void)
{
	if (m_covers == NULL) return;

	LockMutex lock(m_mutex);
	_updateAllTargets(true);
}

void CCoverFlow::stopCoverLoader(bool empty)
{
	m_loadingCovers = false;
	m_moved = true;
	if(coverLoaderThread != LWP_THREAD_NULL)
	{
		if(LWP_ThreadIsSuspended(coverLoaderThread))
			LWP_ResumeThread(coverLoaderThread);
		while(m_coverThrdBusy)
			usleep(50);

		LWP_JoinThread(coverLoaderThread, NULL);
		coverLoaderThread = LWP_THREAD_NULL;
		if(empty)
		{
			for(u32 i = 0; i < m_items.size(); ++i)
			{
				TexHandle.Cleanup(m_items[i].texture);
				m_items[i].state = STATE_Loading;
			}
		}
		//gprintf("Coverflow stopped!\n");
	}
}

void CCoverFlow::startCoverLoader(void)
{
	if(m_covers == NULL || coverLoaderThread != LWP_THREAD_NULL || m_loadingCovers)
		return;

	m_loadingCovers = true;
	m_moved = true;

	LWP_CreateThread(&coverLoaderThread, _coverLoader, this, coverThreadStack, coverThreadStackSize, 40);
	//gprintf("Coverflow started!\n");
}

void CCoverFlow::clear(void)
{
	stopCoverLoader(true);
	if(m_covers != NULL)
		MEM2_free(m_covers);
	m_covers = NULL;
	m_items.clear();
	//vector<CItem>().swap(m_items);
}

void CCoverFlow::shutdown(void)
{
	gprintf("Cleanup Coverflow\n");
	TexHandle.Cleanup(m_dvdSkin);
	TexHandle.Cleanup(m_dvdSkin_Red);
	TexHandle.Cleanup(m_dvdSkin_Black);
	TexHandle.Cleanup(m_dvdSkin_Yellow);
	TexHandle.Cleanup(m_dvdSkin_GreenOne);
	TexHandle.Cleanup(m_dvdSkin_GreenTwo);
	TexHandle.Cleanup(m_dvdSkin_Clear);
	clear();

	if(m_flipSound != NULL)
		delete m_flipSound;
	m_flipSound = NULL;
	if(m_cancelSound != NULL)
		delete m_cancelSound;
	m_cancelSound = NULL;
	LWP_MutexDestroy(m_mutex);
}

void CCoverFlow::reserve(u32 capacity)
{
	m_items.reserve(capacity);
}

void CCoverFlow::addItem(dir_discHdr *hdr, int playcount, unsigned int lastPlayed)
{
	if (m_covers != NULL) return;
	m_items.push_back(CCoverFlow::CItem(hdr, playcount, lastPlayed));
}

// Draws a plane in the Z-Buffer only.
void CCoverFlow::_drawMirrorZ(void)
{
	GX_LoadPosMtxImm(m_viewMtx, GX_PNMTX0);
	// GX setup
	GX_SetNumChans(0);
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetNumTexGens(0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_DISABLE, GX_COLORNULL);
	GX_SetAlphaUpdate(GX_FALSE);
	GX_SetColorUpdate(GX_FALSE);
	GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_TRUE);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position3f32(-10000.f, 0.f, -10000.f);
	GX_Position3f32(10000.f, 0.f, -10000.f);
	GX_Position3f32(10000.f, 0.f, 10000.f);
	GX_Position3f32(-10000.f, 0.f, 10000.f);
	GX_End();
	GX_SetAlphaUpdate(GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);
}

void CCoverFlow::_effectBg(const TexData * &tex)
{
	Mtx modelViewMtx;
	GXTexObj texObj;
	if(tex == NULL || tex->data == NULL)
		return;
	GX_ClearVtxDesc();
	GX_SetNumTevStages(1);
	GX_SetNumChans(0);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetNumTexGens(1);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetAlphaUpdate(GX_TRUE);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetZMode(GX_DISABLE, GX_ALWAYS, GX_FALSE);
	guMtxIdentity(modelViewMtx);
	GX_LoadPosMtxImm(modelViewMtx, GX_PNMTX0);
	GX_InitTexObj(&texObj, tex->data, tex->width, tex->height, tex->format, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position3f32(0.f, 0.f, 0.f);
	GX_TexCoord2f32(0.f, 0.f);
	GX_Position3f32(640.f, 0.f, 0.f);
	GX_TexCoord2f32(1.f, 0.f);
	GX_Position3f32(640.f, 480.f, 0.f);
	GX_TexCoord2f32(1.f, 1.f);
	GX_Position3f32(0.f, 480.f, 0.f);
	GX_TexCoord2f32(0.f, 1.f);
	GX_End();
}

void CCoverFlow::_effectBlur(bool vertical)
{
	int kSize = m_blurRadius * 2 + 1;
	GXTexObj texObj;
	Mtx mh1;
	Mtx mh2;
	Mtx mh3;
	Mtx mh4;
	Mtx mh5;
	Mtx mh6;
	Mtx modelViewMtx;
	float pixDist = m_blurFactor;
	float w = (float)m_effectTex.width;
	float h = (float)m_effectTex.height;
	float x = 0.f;
	float y = 0.f;
	CColor kGauss7(0xFF * 20 / 64, 0xFF * 15 / 64, 0xFF * 6 / 64, 0xFF * 1 / 64);
	CColor kGauss5(0xFF * 6 / 16, 0xFF * 4 / 16, 0xFF * 1 / 16, 0);
	CColor kGauss3(0xFF * 2 / 4, 0xFF * 1 / 4, 0, 0);
	CColor kBox(0xFF / kSize, 0xFF / kSize, 0xFF / kSize, 0xFF / kSize);
	Mtx44 projMtx;

	GX_SetScissor(0, 0, m_effectTex.width, m_effectTex.height);
	GX_SetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
	GX_InvVtxCache();
	GX_InvalidateTexAll();
	GX_InitTexObj(&texObj, m_effectTex.data, m_effectTex.width, m_effectTex.height, m_effectTex.format, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);
	GX_SetNumTevStages(kSize);
	GX_SetNumTexGens(kSize);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	guMtxIdentity(mh1);
	guMtxIdentity(mh2);
	guMtxIdentity(mh3);
	guMtxIdentity(mh4);
	guMtxIdentity(mh5);
	guMtxIdentity(mh6);
	if (vertical)
	{
		guMtxRowCol(mh1, 0, 3) = pixDist * -1.f / w;
		guMtxRowCol(mh2, 0, 3) = pixDist * 1.f / w;
		guMtxRowCol(mh3, 0, 3) = pixDist * -2.f / w;
		guMtxRowCol(mh4, 0, 3) = pixDist * 2.f / w;
		guMtxRowCol(mh5, 0, 3) = pixDist * -3.f / w;
		guMtxRowCol(mh6, 0, 3) = pixDist * 3.f / w;
	}
	else
	{
		guMtxRowCol(mh1, 1, 3) = pixDist * -1.f / h;
		guMtxRowCol(mh2, 1, 3) = pixDist * 1.f / h;
		guMtxRowCol(mh3, 1, 3) = pixDist * -2.f / h;
		guMtxRowCol(mh4, 1, 3) = pixDist * 2.f / h;
		guMtxRowCol(mh5, 1, 3) = pixDist * -3.f / h;
		guMtxRowCol(mh6, 1, 3) = pixDist * 3.f / h;
	}
	GX_LoadTexMtxImm(mh1, GX_TEXMTX1, GX_MTX2x4);
	GX_LoadTexMtxImm(mh2, GX_TEXMTX2, GX_MTX2x4);
	GX_LoadTexMtxImm(mh3, GX_TEXMTX3, GX_MTX2x4);
	GX_LoadTexMtxImm(mh4, GX_TEXMTX4, GX_MTX2x4);
	GX_LoadTexMtxImm(mh3, GX_TEXMTX5, GX_MTX2x4);
	GX_LoadTexMtxImm(mh4, GX_TEXMTX6, GX_MTX2x4);
	GX_SetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX1);
	GX_SetTexCoordGen(GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX2);
	GX_SetTexCoordGen(GX_TEXCOORD3, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX3);
	GX_SetTexCoordGen(GX_TEXCOORD4, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX4);
	GX_SetTexCoordGen(GX_TEXCOORD5, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX5);
	GX_SetTexCoordGen(GX_TEXCOORD6, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX6);
	switch (m_blurRadius)
	{
		case 1:
			GX_SetTevKColor(GX_KCOLOR0, kGauss3);
			break;
		case 2:
			GX_SetTevKColor(GX_KCOLOR0, kGauss5);
			break;
		case 3:
			GX_SetTevKColor(GX_KCOLOR0, kGauss7);
			break;
		default:
			GX_SetTevKColor(GX_KCOLOR0, kBox);
	}
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0_R);
	GX_SetTevKColorSel(GX_TEVSTAGE1, GX_TEV_KCSEL_K0_G);
	GX_SetTevKColorSel(GX_TEVSTAGE2, GX_TEV_KCSEL_K0_G);
	GX_SetTevKColorSel(GX_TEVSTAGE3, GX_TEV_KCSEL_K0_B);
	GX_SetTevKColorSel(GX_TEVSTAGE4, GX_TEV_KCSEL_K0_B);
	GX_SetTevKColorSel(GX_TEVSTAGE5, GX_TEV_KCSEL_K0_A);
	GX_SetTevKColorSel(GX_TEVSTAGE6, GX_TEV_KCSEL_K0_A);
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_R);
	GX_SetTevKAlphaSel(GX_TEVSTAGE1, GX_TEV_KASEL_K0_G);
	GX_SetTevKAlphaSel(GX_TEVSTAGE2, GX_TEV_KASEL_K0_G);
	GX_SetTevKAlphaSel(GX_TEVSTAGE3, GX_TEV_KASEL_K0_B);
	GX_SetTevKAlphaSel(GX_TEVSTAGE4, GX_TEV_KASEL_K0_B);
	GX_SetTevKAlphaSel(GX_TEVSTAGE5, GX_TEV_KASEL_K0_A);
	GX_SetTevKAlphaSel(GX_TEVSTAGE6, GX_TEV_KASEL_K0_A);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, GX_CC_ZERO);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_KONST, GX_CA_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	for (int i = 1; i < kSize; ++i)
	{
		GX_SetTevOrder(GX_TEVSTAGE0 + i, GX_TEXCOORD0 + i, GX_TEXMAP0, GX_COLORNULL);
		GX_SetTevColorIn(GX_TEVSTAGE0 + i, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, GX_CC_CPREV);
		GX_SetTevAlphaIn(GX_TEVSTAGE0 + i, GX_CA_ZERO, GX_CA_TEXA, GX_CA_KONST, GX_CA_APREV);
		GX_SetTevColorOp(GX_TEVSTAGE0 + i, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaOp(GX_TEVSTAGE0 + i, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	}
	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetZMode(GX_DISABLE, GX_ALWAYS, GX_FALSE);
	GX_SetViewport(0, 0, 640.f, 480.f, 0.f, 1.f);
	guOrtho(projMtx, 0, 480.f + 0, 0, 640.f + 0, 0.f, 1000.0f);
	GX_LoadProjectionMtx(projMtx, GX_ORTHOGRAPHIC);
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetAlphaUpdate(GX_TRUE);
	GX_SetCullMode(GX_CULL_NONE);
	guMtxIdentity(modelViewMtx);
	GX_LoadPosMtxImm(modelViewMtx, GX_PNMTX0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position3f32(x, y, 0.f);
	GX_TexCoord2f32(0.f, 0.f);
	GX_Position3f32(x + w, y, 0.f);
	GX_TexCoord2f32(1.f, 0.f);
	GX_Position3f32(x + w, y + h, 0.f);
	GX_TexCoord2f32(1.f, 1.f);
	GX_Position3f32(x, y + h, 0.f);
	GX_TexCoord2f32(0.f, 1.f);
	GX_End();
	m_vid.renderToTexture(m_effectTex, true);
}

bool CCoverFlow::_effectVisible(void)
{
	SLayout &lo = m_selected ? m_loSelected : m_loNormal;
	return (m_mirrorAlpha > 0.01f && m_mirrorBlur)
		|| lo.shadowColorCenter.a > 0 || lo.shadowColorBeg.a > 0
		|| lo.shadowColorEnd.a > 0 || lo.shadowColorOff.a > 0;
}

void CCoverFlow::makeEffectTexture(const TexData * &bg)
{
	if (!_effectVisible()) return;
	int aa = 8;

	GX_SetDither(GX_DISABLE);
	m_vid.setAA(aa, true, m_effectTex.width, m_effectTex.height);
	for (int i = 0; i < aa; ++i)
	{
		m_vid.prepareAAPass(i);
		m_vid.setup2DProjection(false, true);
		_effectBg(bg);
		if (m_mirrorBlur)
			_draw(CCoverFlow::CFDR_NORMAL, true, false);
		m_vid.shiftViewPort(m_shadowX, m_shadowY);
		_draw(CCoverFlow::CFDR_SHADOW, false, true);
		m_vid.renderAAPass(i);
	}
	GX_SetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
	GX_InvVtxCache();
	GX_InvalidateTexAll();
	m_vid.setup2DProjection(false, true);
	GX_SetViewport(0.f, 0.f, (float)m_effectTex.width, (float)m_effectTex.height, 0.f, 1.f);
	GX_SetScissor(0, 0, m_effectTex.width, m_effectTex.height);
	m_vid.drawAAScene();
	m_vid.renderToTexture(m_effectTex, true);
	_effectBlur(false);
	_effectBlur(true);
	GX_SetDither(GX_ENABLE);
}

void CCoverFlow::drawEffect(void)
{
	if (m_covers == NULL) return;

	if (_effectVisible())
	{
		Mtx modelViewMtx;
		GXTexObj texObj;
		float w = 640.f;
		float h = 480.f;
		float x = 0.f;
		float y = 0.f;

		GX_SetNumTevStages(1);
		GX_SetNumChans(0);
		GX_ClearVtxDesc();
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
		GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
		GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
		GX_SetNumTexGens(1);
		GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
		GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
		GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
		GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
		GX_SetAlphaUpdate(GX_FALSE);
		GX_SetCullMode(GX_CULL_NONE);
		GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_FALSE);
		guMtxIdentity(modelViewMtx);
		GX_LoadPosMtxImm(modelViewMtx, GX_PNMTX0);
		GX_InitTexObj(&texObj, m_effectTex.data, m_effectTex.width, m_effectTex.height, m_effectTex.format, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_LoadTexObj(&texObj, GX_TEXMAP0);
		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32(x, y, -999.f);
		GX_TexCoord2f32(0.f, 0.f);
		GX_Position3f32(x + w, y, -999.f);
		GX_TexCoord2f32(1.f, 0.f);
		GX_Position3f32(x + w, y + h, -999.f);
		GX_TexCoord2f32(1.f, 1.f);
		GX_Position3f32(x, y + h, -999.f);
		GX_TexCoord2f32(0.f, 1.f);
		GX_End();
	}
	if (!m_mirrorBlur)
		_draw(CCoverFlow::CFDR_NORMAL, true, true);
}

void CCoverFlow::draw(void)
{
	_draw();
}

void CCoverFlow::drawText(bool withRectangle)
{
	LockMutex lock(m_mutex);
	Vector3D up(0.f, 1.f, 0.f);
	Vector3D dir(m_cameraAim);
	Vector3D pos(m_cameraPos);

	if (m_covers == NULL) return;
	if (m_fontColor.a == 0) return;

	pos += _cameraMoves();
	// Camera
	GX_LoadProjectionMtx(m_projMtx, GX_PERSPECTIVE);
	guLookAt(m_viewMtx, &pos, &up, &dir);
	// Text
	GX_SetNumChans(1);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetZMode(GX_DISABLE, GX_LEQUAL, GX_FALSE);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	for (u32 i = 0; i < m_range; ++i)
	{
		_drawTitle(loopNum((i & 1) != 0 ? m_range / 2 - (i + 1) / 2 : m_range / 2 + i / 2, m_range), true, withRectangle);
		_drawTitle(loopNum((i & 1) != 0 ? m_range / 2 - (i + 1) / 2 : m_range / 2 + i / 2, m_range), false, withRectangle);
	}
}

void CCoverFlow::hideCover(void)
{
	m_hideCover = true;
}

void CCoverFlow::showCover(void)
{
	m_hideCover = false;
}

inline int innerToOuter(int i, int range)
{
	return loopNum((i & 1) != 0 ? range / 2 - (i + 1) / 2 : range / 2 + i / 2, range);
}

void CCoverFlow::_draw(DrawMode dm, bool mirror, bool blend)
{
	LockMutex lock(m_mutex);
	Vector3D up(0.f, 1.f, 0.f);
	Vector3D dir(m_cameraAim);
	Vector3D pos(m_cameraPos);

	if (mirror && m_mirrorAlpha <= 0.f) return;
	if (m_covers == NULL) return;

	pos += _cameraMoves();
	// GX setup
	GX_ClearVtxDesc();
	GX_SetNumTevStages(1);
	if (dm == CCoverFlow::CFDR_NORMAL)
	{
		GX_SetNumChans(0);
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
		GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
		GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
		GX_SetNumTexGens(1);
		GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_KONST, GX_CC_TEXC, GX_CC_ZERO);
		GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_KONST, GX_CA_TEXA, GX_CA_ZERO);
		GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
		GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	}
	else
	{
		GX_SetNumChans(1);
		GX_SetNumTexGens(0);
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
		GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_KONST);
		GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
		GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLORNULL);
	}
	GX_SetTevKColor(GX_KCOLOR0, CColor(0xFF, 0xFF, 0xFF, 0xFF));
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);
	if (blend)
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	else
		GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetAlphaUpdate(GX_TRUE);
	// Camera
	GX_LoadProjectionMtx(m_projMtx, GX_PERSPECTIVE);
	guLookAt(m_viewMtx, &pos, &up, &dir);
	// Covers
	GX_SetCullMode(m_box ? (mirror ? GX_CULL_BACK : GX_CULL_FRONT) : GX_CULL_NONE);
	if (dm == CCoverFlow::CFDR_SHADOW)
	{
		// Shadow
		GX_SetZMode(GX_DISABLE, GX_LEQUAL, GX_FALSE);
		for (int x = (int)m_columns - 3; x >= 0; --x)
		{
			int xx = innerToOuter(x, m_columns);
			if (m_rows >= 3)
				for (int y = (int)m_rows - 3; y >= 0; --y)
					_drawCover(xx * m_rows + innerToOuter(y, m_rows), mirror, dm);
			else
				_drawCover(xx, mirror, dm);
		}
	}
	else
	{
		// Normal
		GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_TRUE);
		for (int x = 0; x < (int)m_columns - 2; ++x)
		{
			int xx = innerToOuter(x, m_columns);
			if (m_rows >= 3)
				for (int y = 0; y < (int)m_rows - 2; ++y)
					_drawCover(xx * m_rows + innerToOuter(y, m_rows), mirror, dm);
			else
				_drawCover(xx, mirror, dm);
		}
		// Vanishing covers
		GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_FALSE);
		for (u32 y = 0; y < m_rows; ++y)
		{
			_drawCover(y, mirror, dm);
			_drawCover((m_columns - 1) * m_rows + y, mirror, dm);
		}
		if (m_rows >= 3)
			for (u32 x = 1; x < m_columns - 1; ++x)
			{
				_drawCover(x * m_rows, mirror, dm);
				_drawCover(x * m_rows + m_rows - 1, mirror, dm);
			}
	}
}

Vector3D CCoverFlow::_cameraMoves(void)
{
	SLayout &lo = m_selected ? m_loSelected : m_loNormal;
	float tick = (float)m_tickCount * 0.005f;
	return Vector3D(cos(tick * lo.cameraOscSpeed.x) * lo.cameraOscAmp.x,
		cos(tick * lo.cameraOscSpeed.y) * lo.cameraOscAmp.y,
		cos(tick * lo.cameraOscSpeed.z) * lo.cameraOscAmp.z);
}

Vector3D CCoverFlow::_coverMovesA(void)//angle
{
	SLayout &lo = m_selected ? m_loSelected : m_loNormal;
	float tick = (float)m_tickCount * 0.005f;
	return Vector3D(cos(tick * lo.coverOscASpeed.x) * lo.coverOscAAmp.x,
		sin(tick * lo.coverOscASpeed.y) * lo.coverOscAAmp.y,
		cos(tick * lo.coverOscASpeed.z) * lo.coverOscAAmp.z);
}

Vector3D CCoverFlow::_coverMovesP(void)//position
{
	SLayout &lo = m_selected ? m_loSelected : m_loNormal;
	float tick = (float)m_tickCount * 0.005f;
	return Vector3D(cos(tick * lo.coverOscPSpeed.x) * lo.coverOscPAmp.x,
		sin(tick * lo.coverOscPSpeed.y) * lo.coverOscPAmp.y,
		cos(tick * lo.coverOscPSpeed.z) * lo.coverOscPAmp.z);
}

void CCoverFlow::_drawTitle(int i, bool mirror, bool rectangle)
{
	Mtx modelMtx;
	Mtx modelViewMtx;
	Vector3D rotAxis(0.f, 1.f, 0.f);
	CColor color(m_fontColor);

	if (m_hideCover) return;
	if (m_covers[i].txtColor == 0) return;

	color.a = mirror ? (u8)((float)m_covers[i].txtColor * m_txtMirrorAlpha) : m_covers[i].txtColor;
	
	// rectangle is never used. in menu.cpp _mainLoopCommon() adjusting is never set true
	// but if it would be. rectangle is the same as using a frame for the text as when the gui draws a label.
	// when using a frame the text style is used. otherwise it is not used.
	if (rectangle && !mirror)
	{
		// GX setup
		GX_SetNumTevStages(1);
		GX_SetNumChans(1);
		GX_ClearVtxDesc();
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
		GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
		GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
		GX_SetNumTexGens(0);
		GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
		GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
		GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
		// 
		Mtx rotMtx;
		guMtxIdentity(modelMtx);
		guMtxScaleApply(modelMtx, modelMtx, 0.005f, 0.005f, 0.005f);
		guMtxRotDeg(rotMtx, 'Y', m_covers[i].txtAngle);
		guMtxConcat(rotMtx, modelMtx, modelMtx);
		guMtxTransApply(modelMtx, modelMtx, m_covers[i].txtPos.x, mirror ? -m_covers[i].txtPos.y : m_covers[i].txtPos.y, m_covers[i].txtPos.z);
		guMtxConcat(m_viewMtx, modelMtx, modelViewMtx);
		GX_LoadPosMtxImm(modelViewMtx, GX_PNMTX0);
		const SLayout &lo = m_selected ? m_loSelected : m_loNormal;
		u16 s = (u32)i == m_range / 2 ? lo.txtCenterStyle : lo.txtSideStyle;
		float x;
		float y;
		float w = (u32)i == m_range / 2 ? lo.txtCenterWidth : lo.txtSideWidth;
		float h = m_font.lineSpacing;
		if ((s & FTGX_JUSTIFY_CENTER) != 0)
			x = -w * 0.5f;
		else if ((s & FTGX_JUSTIFY_RIGHT) != 0)
			x = -w;
		else
			x = 0.f;
		if ((s & FTGX_ALIGN_MIDDLE) != 0)
			y = -h * 0.5f;
		else if ((s & FTGX_ALIGN_TOP) != 0)
			y = -h;
		else
			y = 0.f;
		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32(x, y, 0.f);
		GX_Color4u8(color.r, color.g, color.b, color.a / 2);
		GX_Position3f32(x + w, y, 0.f);
		GX_Color4u8(color.r, color.g, color.b, color.a / 2);
		GX_Position3f32(x + w, y + h, 0.f);
		GX_Color4u8(color.r, color.g, color.b, color.a / 2);
		GX_Position3f32(x, y + h, 0.f);
		GX_Color4u8(color.r, color.g, color.b, color.a / 2);
		GX_End();
	}
	m_covers[i].title.setColor(color);
	m_font.font->reset();
	m_font.font->setXScale(0.005f);
	m_font.font->setYScale(mirror ? 0.005f : -0.005f);
	GX_ClearVtxDesc();
	GX_SetNumTevStages(1);
	GX_SetNumChans(1);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetNumTexGens(1);
	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	// 
	guMtxIdentity(modelMtx);
	guMtxRotAxisDeg(modelMtx, &rotAxis, m_covers[i].txtAngle);
	guMtxTransApply(modelMtx, modelMtx, m_covers[i].txtPos.x, mirror ? -m_covers[i].txtPos.y : m_covers[i].txtPos.y, m_covers[i].txtPos.z);
	guMtxConcat(m_viewMtx, modelMtx, modelViewMtx);
	GX_LoadPosMtxImm(modelViewMtx, GX_PNMTX0);
	m_covers[i].title.draw();
}

void CCoverFlow::_drawCover(int i, bool mirror, CCoverFlow::DrawMode dm)
{
	Mtx modelMtx;
	Mtx rotMtx;
	Mtx modelViewMtx;
	Vector3D osc;
	Vector3D oscP;

	if (m_hideCover)
		return;
	if (m_covers[i].color.a == 0 || (dm == CCoverFlow::CFDR_SHADOW && m_covers[i].shadowColor.a == 0))
		return;
	if (dm == CCoverFlow::CFDR_STENCIL && (i > 0xFF || _invisibleCover(i / m_rows, i % m_rows)))
		return;
	if ((u32)i == m_range / 2)
	{
		osc = _coverMovesA();
		oscP = _coverMovesP();
	}
	// 
	guMtxIdentity(modelMtx);
	guMtxScaleApply(modelMtx, modelMtx, m_covers[i].scale.x, m_covers[i].scale.y, m_covers[i].scale.z);
	if (dm == CCoverFlow::CFDR_SHADOW)
		guMtxScaleApply(modelMtx, modelMtx, 1.f + (m_shadowScale - 1.f) * g_boxSize.y / g_boxSize.x, m_shadowScale, m_shadowScale);
	guMtxRotDeg(rotMtx, 'Z', m_covers[i].angle.z + osc.z);
	guMtxConcat(rotMtx, modelMtx, modelMtx);
	guMtxRotDeg(rotMtx, 'X', m_covers[i].angle.x + osc.x);
	guMtxConcat(rotMtx, modelMtx, modelMtx);
	guMtxRotDeg(rotMtx, 'Y', m_covers[i].angle.y + osc.y);
	guMtxConcat(rotMtx, modelMtx, modelMtx);
	guMtxTransApply(modelMtx, modelMtx, m_covers[i].pos.x + oscP.x, m_covers[i].pos.y + oscP.y + g_coverYCenter, m_covers[i].pos.z + oscP.z);
	if (mirror)
		guMtxScaleApply(modelMtx, modelMtx, 1.f, -1.f, 1.f);
	guMtxConcat(m_viewMtx, modelMtx, modelViewMtx);
	GX_LoadPosMtxImm(modelViewMtx, GX_PNMTX0);
	// Cover
	if (m_box)
		_drawCoverBox(i, mirror, dm);
	else
		_drawCoverFlat(i, mirror, dm);
}

const TexData *CCoverFlow::_coverTexture(int i)
{
	if(m_items[i].texture.data == NULL)
	{
		if(m_box)
			return (m_items[i].state == STATE_Loading) ? &m_boxLoadingTexture : &m_boxNoCoverTexture;
		else
			return (m_items[i].state == STATE_Loading) ? &m_flatLoadingTexture : &m_flatNoCoverTexture;
	}
	return &m_items[i].texture;
}

void CCoverFlow::_drawCoverFlat(int i, bool mirror, CCoverFlow::DrawMode dm)
{
	GXTexObj texObj;
	const TexData *tex = _coverTexture(m_covers[i].index);
	bool boxTex = m_items[m_covers[i].index].boxTexture && !!m_items[m_covers[i].index].texture.data;

	switch (dm)
	{
		case CCoverFlow::CFDR_NORMAL:
		{
			CColor color(m_covers[i].color);
			if (mirror)
				color.a = (u8)((float)color.a * m_mirrorAlpha);
			GX_SetTevKColor(GX_KCOLOR0, color);
			break;
		}
		case CCoverFlow::CFDR_STENCIL:
			GX_SetTevKColor(GX_KCOLOR0, CColor(i + 1, 0xFF, 0xFF, 0xFF));
			break;
		case CCoverFlow::CFDR_SHADOW:
		{
			CColor color(m_covers[i].shadowColor);
			if (mirror)
				color.a = (u8)((float)color.a * m_mirrorAlpha);
			GX_SetTevKColor(GX_KCOLOR0, color);
			break;
		}
	}
	if(dm == CCoverFlow::CFDR_NORMAL)
	{
		GX_InitTexObj(&texObj, tex->data, tex->width, tex->height, tex->format, GX_CLAMP, GX_CLAMP, GX_FALSE);
		if(tex->maxLOD > 0)
			GX_InitTexObjLOD(&texObj, GX_LIN_MIP_LIN, GX_LINEAR, 0.f, (float)tex->maxLOD, mirror ? 1.f : m_lodBias, GX_FALSE, m_edgeLOD ? GX_TRUE : GX_FALSE, m_aniso);
		GX_LoadTexObj(&texObj, GX_TEXMAP0);
	}
	GX_Begin(GX_QUADS, GX_VTXFMT0, g_flatCoverMeshSize);
	for (u32 j = 0; j < g_flatCoverMeshSize; ++j)
	{
		GX_Position3f32(g_flatCoverMesh[j].pos.x, g_flatCoverMesh[j].pos.y, g_flatCoverMesh[j].pos.z);
		if (dm == CCoverFlow::CFDR_NORMAL)
		{
			if (boxTex)
				GX_TexCoord2f32(g_flatCoverBoxTex[j].x, g_flatCoverBoxTex[j].y);
			else
				GX_TexCoord2f32(g_flatCoverMesh[j].texCoord.x, g_flatCoverMesh[j].texCoord.y);
		}
	}
	GX_End();
}

bool CCoverFlow::checkCoverColor(const char *gameID, const char *checkID[], u32 len)
{
	for(u32 num = 0; num < len; ++num)
	{
		if (strncmp(gameID, checkID[num], strlen(checkID[num])) == 0)
			return true;
	}
	return false;
}

u32 CCoverFlow::InternalCoverColor(const char *ID, u32 DefCaseColor)
{
	if(checkCoverColor(ID, red, red_len))
		return 0xFF0000;
	else if(checkCoverColor(ID, black, black_len))
		return 0x000000;
	else if(checkCoverColor(ID, yellow, yellow_len))
		return 0xFCFF00;
	else if(checkCoverColor(ID, greenOne, greenOne_len))
		return 0x01A300;
	else if(checkCoverColor(ID, greenTwo, greenTwo_len))
		return 0x00E360;
	return DefCaseColor;
}

void CCoverFlow::_drawCoverBox(int i, bool mirror, CCoverFlow::DrawMode dm)
{
	CColor color;
	GXTexObj texObj;
	const TexData *tex = _coverTexture(m_covers[i].index);
	bool flatTex = !m_items[m_covers[i].index].boxTexture && !!m_items[m_covers[i].index].texture.data;

	switch (dm)
	{
		case CCoverFlow::CFDR_NORMAL:
			color = m_covers[i].color;
			if (mirror)
				color.a = (u8)((float)color.a * m_mirrorAlpha);
			GX_SetTevKColor(GX_KCOLOR0, color);
			break;
		case CCoverFlow::CFDR_STENCIL:
			GX_SetTevKColor(GX_KCOLOR0, CColor(i + 1, 0xFF, 0xFF, 0xFF));
			break;
		case CCoverFlow::CFDR_SHADOW:
			color = m_covers[i].shadowColor;
			if (mirror)
				color.a = (u8)((float)color.a * m_mirrorAlpha);
			GX_SetTevKColor(GX_KCOLOR0, color);
			break;
	}
	/* this draws the colored spine of the box */
	if(dm == CCoverFlow::CFDR_NORMAL)
	{
		// set dvd spine color texture, depending on game
		switch(m_items[m_covers[i].index].hdr->casecolor)
		{
			case 0x000000:
			case 0x181919:
				GX_InitTexObj(&texObj, m_dvdSkin_Black.data, m_dvdSkin_Black.width, m_dvdSkin_Black.height, m_dvdSkin_Black.format, GX_CLAMP, GX_CLAMP, GX_FALSE);
				break;
			case 0xFF0000:
				GX_InitTexObj(&texObj, m_dvdSkin_Red.data, m_dvdSkin_Red.width, m_dvdSkin_Red.height, m_dvdSkin_Red.format, GX_CLAMP, GX_CLAMP, GX_FALSE);
				break;
			case 0xFCFF00:
				GX_InitTexObj(&texObj, m_dvdSkin_Yellow.data, m_dvdSkin_Yellow.width, m_dvdSkin_Yellow.height, m_dvdSkin_Yellow.format, GX_CLAMP, GX_CLAMP, GX_FALSE);
				break;
			case 0x01A300:
				GX_InitTexObj(&texObj, m_dvdSkin_GreenOne.data, m_dvdSkin_GreenOne.width, m_dvdSkin_GreenOne.height, m_dvdSkin_GreenOne.format, GX_CLAMP, GX_CLAMP, GX_FALSE);
				break;
			case 0x00E360:
				GX_InitTexObj(&texObj, m_dvdSkin_GreenTwo.data, m_dvdSkin_GreenTwo.width, m_dvdSkin_GreenTwo.height, m_dvdSkin_GreenTwo.format, GX_CLAMP, GX_CLAMP, GX_FALSE);
				break;
			case 0x111111:
				GX_InitTexObj(&texObj, m_dvdSkin_Clear.data, m_dvdSkin_Clear.width, m_dvdSkin_Clear.height, m_dvdSkin_Clear.format, GX_CLAMP, GX_CLAMP, GX_FALSE);
				break;
			default:
				GX_InitTexObj(&texObj, m_dvdSkin.data, m_dvdSkin.width, m_dvdSkin.height, m_dvdSkin.format, GX_CLAMP, GX_CLAMP, GX_FALSE);
				break;
		}
		GX_LoadTexObj(&texObj, GX_TEXMAP0);
	}
	GX_Begin(GX_QUADS, GX_VTXFMT0, g_boxMeshQSize);// Quads
	for (u32 j = 0; j < g_boxMeshQSize; ++j)
	{
		GX_Position3f32(g_boxMeshQ[j].pos.x, g_boxMeshQ[j].pos.y, g_boxMeshQ[j].pos.z);
		if (dm == CCoverFlow::CFDR_NORMAL)
			GX_TexCoord2f32(g_boxMeshQ[j].texCoord.x, g_boxMeshQ[j].texCoord.y);
	}
	GX_End();
	GX_Begin(GX_TRIANGLES, GX_VTXFMT0, g_boxMeshTSize);// triangles
	for (u32 j = 0; j < g_boxMeshTSize; ++j)
	{
		GX_Position3f32(g_boxMeshT[j].pos.x, g_boxMeshT[j].pos.y, g_boxMeshT[j].pos.z);
		if (dm == CCoverFlow::CFDR_NORMAL)
			GX_TexCoord2f32(g_boxMeshT[j].texCoord.x, g_boxMeshT[j].texCoord.y);
	}
	GX_End();
	/* draw back of cover and title spine texture */
	if (dm == CCoverFlow::CFDR_NORMAL)
	{
		const TexData *myTex = tex;
		/* if we have front cover texture only then draw back and spine textures using the default box no cover texture */
		if(flatTex)
			myTex = &m_boxNoCoverTexture;
		GX_InitTexObj(&texObj, myTex->data, myTex->width, myTex->height, myTex->format, GX_CLAMP, GX_CLAMP, GX_FALSE);
		if (myTex->maxLOD > 0)
			GX_InitTexObjLOD(&texObj, GX_LIN_MIP_LIN, GX_LINEAR, 0.f, (float)myTex->maxLOD, mirror ? 1.f : m_lodBias, GX_FALSE, m_edgeLOD ? GX_TRUE : GX_FALSE, m_aniso);
		GX_LoadTexObj(&texObj, GX_TEXMAP0);
	}
	GX_Begin(GX_QUADS, GX_VTXFMT0, g_boxBackCoverMeshSize);
	for (u32 j = 0; j < g_boxBackCoverMeshSize; ++j)
	{
		GX_Position3f32(g_boxBackCoverMesh[j].pos.x, g_boxBackCoverMesh[j].pos.y, g_boxBackCoverMesh[j].pos.z);
		if (dm == CCoverFlow::CFDR_NORMAL)
			GX_TexCoord2f32(g_boxBackCoverMesh[j].texCoord.x, g_boxBackCoverMesh[j].texCoord.y);
	}
	GX_End();
	/* draw front of cover texture */
	/* if front cover only we need to change texObj from box no cover texture to the front cover texture */
	if (dm == CCoverFlow::CFDR_NORMAL && flatTex)
	{
		GX_InitTexObj(&texObj, tex->data, tex->width, tex->height, tex->format, GX_CLAMP, GX_CLAMP, GX_FALSE);
		if (tex->maxLOD > 0)
			GX_InitTexObjLOD(&texObj, GX_LIN_MIP_LIN, GX_LINEAR, 0.f, (float)tex->maxLOD, mirror ? 1.f : m_lodBias, GX_FALSE, m_edgeLOD ? GX_TRUE : GX_FALSE, m_aniso);
		GX_LoadTexObj(&texObj, GX_TEXMAP0);
	}
	GX_Begin(GX_QUADS, GX_VTXFMT0, g_boxCoverMeshSize);
	for (u32 j = 0; j < g_boxCoverMeshSize; ++j)
	{
		GX_Position3f32(g_boxCoverMesh[j].pos.x, g_boxCoverMesh[j].pos.y, g_boxCoverMesh[j].pos.z);
		if (dm == CCoverFlow::CFDR_NORMAL)
		{
			if (flatTex)
				GX_TexCoord2f32(g_boxCoverFlatTex[j].x, g_boxCoverFlatTex[j].y);
			else
				GX_TexCoord2f32(g_boxCoverMesh[j].texCoord.x, g_boxCoverMesh[j].texCoord.y);
		}
	}
	GX_End();
}

void CCoverFlow::_loadCover(int i, int item)
{
	m_covers[i].index = item;
	m_covers[i].title.setText(m_font, m_items[item].hdr->title);
}

const char *CCoverFlow::getId(void) const
{
	if (m_covers == NULL || m_items.empty()) return "";
	return m_items[loopNum(m_covers[m_range / 2].index + m_jump, m_items.size())].hdr->id;
}

const char *CCoverFlow::getNextId(void) const
{
	if (m_covers == NULL || m_items.empty()) return "";
	return m_items[loopNum(m_covers[m_range / 2].index + m_jump + 1, m_items.size())].hdr->id;
}

const dir_discHdr * CCoverFlow::getHdr(void) const
{
	if (m_covers == NULL || m_items.empty()) return NULL;
	return m_items[loopNum(m_covers[m_range / 2].index + m_jump, m_items.size())].hdr;
}

const dir_discHdr * CCoverFlow::getNextHdr(void) const
{
	if (m_covers == NULL || m_items.empty()) return NULL;
	return m_items[loopNum(m_covers[m_range / 2].index + m_jump + 1, m_items.size())].hdr;
}

const dir_discHdr * CCoverFlow::getSpecificHdr(u32 place) const
{
	if (m_covers == NULL || m_items.empty() || place >= m_items.size()) return NULL;
	return m_items[place].hdr;
}

wstringEx CCoverFlow::getTitle(void) const
{
	if (m_covers == NULL) return L"";

	return m_items[m_covers[m_range / 2].index].hdr->title;
}

u64 CCoverFlow::getChanTitle(void) const
{
	if (m_covers == NULL || m_items.empty()) return 0;

	return TITLE_ID(m_items[loopNum(m_covers[m_range / 2].index + m_jump, m_items.size())].hdr->settings[0],
					m_items[loopNum(m_covers[m_range / 2].index + m_jump, m_items.size())].hdr->settings[1]);
}

bool CCoverFlow::getRenderTex(void)
{
	return m_renderTex;
}

void CCoverFlow::setRenderTex(bool val)
{
	m_renderTex = val;
}

/* this is used for smallbox homebrew icon png's that have alpha transparency */
/* cover textures can't have transparency. this creates a new texture by filling the transparent parts */
void CCoverFlow::RenderTex(void)
{
	if(m_renderingTex != NULL && m_renderingTex->data != NULL)
	{
		DrawTexture(m_renderingTex);
		m_vid.renderToTexture(*m_renderingTex, true);
	}
	setRenderTex(false);
}

bool CCoverFlow::select(void)
{
	if (m_covers == NULL || m_jump != 0) return false;
	if (m_selected) return true;

	LockMutex lock(m_mutex);
	int curPos = (int)m_range / 2;
	for(int chan = WPAD_MAX_WIIMOTES-1; chan >= 0; chan--)
		if ((u32)m_mouse[chan] < m_range && m_mouse[chan] != curPos)
		{
			int j;
			if (m_rows >= 3)
				j = ((m_mouse[chan] / m_rows - 1) * (m_rows - 2) + m_mouse[chan] % m_rows - 1)
					- ((curPos / m_rows - 1) * (m_rows - 2) + curPos % m_rows - 1);
			else
				j = m_mouse[chan] - (int)(m_range / 2);
			_setJump(j);
		}
	m_cameraPos += _cameraMoves();
	m_covers[m_range / 2].angle += _coverMovesA();
	m_covers[m_range / 2].pos += _coverMovesP();
	m_selected = true;
	m_covers[m_range / 2].angle -= _coverMovesA();
	m_covers[m_range / 2].pos -= _coverMovesP();
	m_cameraPos -= _cameraMoves();
	_updateAllTargets();
	_playSound(m_selectSound);
	return true;
}

void CCoverFlow::cancel(void)
{
	if (m_covers == NULL) return;

	LockMutex lock(m_mutex);
	_unselect();
	_updateAllTargets();
	_playSound(m_cancelSound);
}

u32 CCoverFlow::_currentPos(void) const
{
	if (m_covers == NULL) return 0;

	return m_covers[m_range / 2].index;
}

void CCoverFlow::_setCurPos(u32 index)
{
	_loadAllCovers(index);
	_updateAllTargets(true);
}

bool CCoverFlow::_setCurPosToCurItem(const char *id, const char *filename, u32 sourceNumber, bool instant)
{
	LockMutex lock(m_mutex);
	u32 i, curPos = _currentPos();

	if(m_items.empty() || (instant && m_covers == NULL))
		return false;
	// 
	for(i = 0; i < m_items.size(); ++i)
	{
		if(strlen(filename) > 0)
		{
			//homebrew folder or rom title.ext
			const char *name = strrchr(m_items[i].hdr->path, '/');
			if(name != NULL && strcasecmp(name + 1, filename) == 0)
				break;
			else if(strcmp(m_items[i].hdr->path, filename) == 0)// scummvm
				break;
		}
		else if(strlen(id) > 0)
		{
			if(strcasecmp(m_items[i].hdr->id, id) == 0)
				break;
		}
		else
		{
			if(m_items[i].hdr->settings[0] == sourceNumber)
				break;
		}
	}
	if(i >= m_items.size())
		return false;
	m_jump = 0;
	if (instant)
	{
		_loadAllCovers(i);
		_updateAllTargets(true);
	}
	else
	{
		int j = (int)i - (int)curPos;
		if (abs(j) <= (int)m_items.size() / 2)
			_setJump(j);
		else
			_setJump(j < 0 ? j + (int)m_items.size() : j - (int)m_items.size());
	}
	return true;
}

void CCoverFlow::_updateAllTargets(bool instant)
{
	m_targetCameraPos = m_selected ? m_loSelected.camera : m_loNormal.camera;
	m_targetCameraAim = m_selected ? m_loSelected.cameraAim : m_loNormal.cameraAim;
	for (u32 i = 0; i < m_range; ++i)
		_updateTarget(i, instant);
}

void CCoverFlow::_updateTarget(int i, bool instant)
{
	SLayout &lo = m_selected ? m_loSelected : m_loNormal;
	int hcenter = m_columns / 2;
	int vcenter = m_rows / 2;
	int x = i / m_rows;
	int y = i % m_rows;
	CCover &cvr = m_covers[i];

	// Left covers
	if (x < hcenter)
	{
		Vector3D deltaAngle(lo.leftDeltaAngle * (float)(hcenter - 1 - x));
		cvr.targetAngle = lo.leftAngle + deltaAngle;
		cvr.targetPos = lo.leftPos + lo.leftSpacer.rotateY(deltaAngle.y * 0.5f) * (float)(hcenter - 1 - x);
		cvr.targetScale = lo.leftScale;
		if (_invisibleCover(x, y))
		{
			cvr.targetColor = CColor(lo.endColor.r, lo.endColor.g, lo.endColor.b, 0);
			cvr.targetShadowColor = CColor(lo.shadowColorEnd.r, lo.shadowColorEnd.g, lo.shadowColorEnd.b, 0);
		}
		else
		{
			u8 a = (x - 1) * 0xFF / max(1, hcenter - 2);
			cvr.targetColor = CColor::interpolate(lo.endColor, lo.begColor, a);
			cvr.targetShadowColor = CColor::interpolate(lo.shadowColorEnd, lo.shadowColorBeg, a);
		}
		cvr.txtTargetAngle = lo.txtLeftAngle;
		cvr.txtTargetPos = lo.txtLeftPos;
		cvr.txtTargetColor = 0;
		cvr.txtColor = 0;//instant fade out if was displayed
		cvr.title.setFrame(lo.txtSideWidth, lo.txtSideStyle, false, instant);
	}
	// Right covers
	else if (x > hcenter)
	{
		Vector3D deltaAngle(lo.rightDeltaAngle * (float)(x - hcenter - 1));
		cvr.targetAngle = lo.rightAngle + deltaAngle;
		cvr.targetPos = lo.rightPos + lo.rightSpacer.rotateY(deltaAngle.y * 0.5f) * (float)(x - hcenter - 1);
		cvr.targetScale = lo.rightScale;
		if (_invisibleCover(x, y))
		{
			cvr.targetColor = CColor(lo.endColor.r, lo.endColor.g, lo.endColor.b, 0);
			cvr.targetShadowColor = CColor(lo.shadowColorEnd.r, lo.shadowColorEnd.g, lo.shadowColorEnd.b, 0);
		}
		else
		{
			u8 a = (m_columns - x - 2) * 0xFF / max(1, hcenter - 2);
			cvr.targetColor = CColor::interpolate(lo.endColor, lo.begColor, a);
			cvr.targetShadowColor = CColor::interpolate(lo.shadowColorEnd, lo.shadowColorBeg, a);
		}
		cvr.txtTargetAngle = lo.txtRightAngle;
		cvr.txtTargetPos = lo.txtRightPos;
		cvr.txtTargetColor = 0;
		cvr.txtColor = 0;//instant fade out if was displayed
		cvr.title.setFrame(lo.txtSideWidth, lo.txtSideStyle, false, instant);
	}
	// New center cover
	else if (y == vcenter)
	{
		cvr.targetColor = 0xFFFFFFFF;
		cvr.targetShadowColor = lo.shadowColorCenter;
		cvr.targetAngle = lo.centerAngle;
		cvr.targetPos = lo.centerPos;
		cvr.targetScale = lo.centerScale;
		cvr.txtTargetColor = 0xFF;
		cvr.txtTargetAngle = lo.txtCenterAngle;
		cvr.txtTargetPos = lo.txtCenterPos;
		cvr.title.setFrame(lo.txtCenterWidth, lo.txtCenterStyle, false, true);
	}
	else // Center of a row
	{
		cvr.targetColor = lo.begColor;
		cvr.targetShadowColor = lo.shadowColorBeg;
		cvr.targetAngle = lo.rowCenterAngle;
		cvr.targetPos = lo.rowCenterPos;
		cvr.targetScale = lo.rowCenterScale;
		cvr.txtTargetColor = 0;
		cvr.txtColor = 0;//instant fade out if was displayed
		if (y < vcenter)
		{
			cvr.txtTargetAngle = lo.txtLeftAngle;
			cvr.txtTargetPos = lo.txtLeftPos;
		}
		else // (y > vcenter)
		{
			cvr.txtTargetAngle = lo.txtRightAngle;
			cvr.txtTargetPos = lo.txtRightPos;
		}
		cvr.title.setFrame(lo.txtSideWidth, lo.txtSideStyle, false, instant);
	}
	// Top row
	if (y < vcenter)
	{
		Vector3D deltaAngle(lo.topDeltaAngle * (float)(vcenter - y - 1));
		cvr.targetAngle += lo.topAngle + deltaAngle;
		cvr.targetPos += lo.topSpacer.rotateX(deltaAngle.x * 0.5f) * (float)(vcenter - y);
		if (_invisibleCover(x, y))
		{
			cvr.targetColor = CColor(lo.endColor.r, lo.endColor.g, lo.endColor.b, 0);
			cvr.targetShadowColor = CColor(lo.shadowColorEnd.r, lo.shadowColorEnd.g, lo.shadowColorEnd.b, 0);
		}
		else
		{
			u8 a = (y - 1) * 0xFF / max(1, vcenter - 2);
			CColor c1(CColor::interpolate(lo.endColor, lo.begColor, a));
			cvr.targetColor = CColor::interpolate(c1, cvr.targetColor, 0x7F);
			CColor c2(CColor::interpolate(lo.shadowColorEnd, lo.shadowColorBeg, a));
			cvr.targetShadowColor = CColor::interpolate(c2, cvr.targetShadowColor, 0x7F);
		}
	}
	// Bottom row
	else if (y > vcenter)
	{
		Vector3D deltaAngle(lo.bottomDeltaAngle * (float)(y - vcenter - 1));
		cvr.targetAngle += lo.bottomAngle + deltaAngle;
		cvr.targetPos += lo.bottomSpacer.rotateX(deltaAngle.x * 0.5f) * (float)(y - vcenter);
		if (_invisibleCover(x, y))
		{
			cvr.targetColor = CColor(lo.endColor.r, lo.endColor.g, lo.endColor.b, 0);
			cvr.targetShadowColor = CColor(lo.shadowColorEnd.r, lo.shadowColorEnd.g, lo.shadowColorEnd.b, 0);
		}
		else
		{
			u8 a = (m_rows - y - 2) * 0xFF / max(1, vcenter - 2);
			CColor c1(CColor::interpolate(lo.endColor, lo.begColor, a));
			cvr.targetColor = CColor::interpolate(c1, cvr.targetColor, 0x7F);
			CColor c2(CColor::interpolate(lo.shadowColorEnd, lo.shadowColorBeg, a));
			cvr.targetShadowColor = CColor::interpolate(c2, cvr.targetShadowColor, 0x7F);
		}
	}
	else if (vcenter > 0 && !_invisibleCover(x, y) && x != hcenter)
	{
		cvr.targetColor = CColor::interpolate(lo.begColor, cvr.targetColor, 0x7F);
		cvr.targetShadowColor = CColor::interpolate(lo.shadowColorBeg, cvr.targetShadowColor, 0x7F);
	}
	// Mouse selection
	for(int chan = WPAD_MAX_WIIMOTES-1; chan >= 0; chan--)
		if (!m_selected && (u32)m_mouse[chan] < m_range)
		{
			if (i == m_mouse[chan])
			{
				cvr.targetColor = 0xFFFFFFFF;
				cvr.targetShadowColor = lo.shadowColorCenter;
				cvr.txtTargetAngle = lo.txtCenterAngle;
				cvr.txtTargetPos = lo.txtCenterPos;
				cvr.txtTargetColor = 0xFF;
				cvr.title.setFrame(lo.txtCenterWidth, lo.txtCenterStyle, false, true);
			}
			else
			{
				cvr.targetColor = lo.mouseOffColor;
				cvr.targetShadowColor = lo.shadowColorOff;
				cvr.txtTargetAngle = m_mouse[chan] > i ? lo.txtLeftAngle : lo.txtRightAngle;
				cvr.txtTargetPos = m_mouse[chan] > i ? lo.txtLeftPos : lo.txtRightPos;
				cvr.txtTargetColor = 0;
				cvr.txtColor = 0;//instant fade out if was displayed
				cvr.title.setFrame(lo.txtSideWidth, lo.txtSideStyle, false, instant);
			}
			if (_invisibleCover(x, y))
			{
				cvr.targetColor.a = 0;
				cvr.targetShadowColor.a = 0;
			}
		}
	// 
	if (instant)
		_instantTarget(i);
}

void CCoverFlow::_instantTarget(int i)
{
	CCover &cvr = m_covers[i];

	cvr.angle = cvr.targetAngle;
	cvr.pos = cvr.targetPos;
	cvr.scale = cvr.targetScale;
	cvr.color = cvr.targetColor;
	cvr.shadowColor = cvr.targetShadowColor;
	cvr.txtAngle = cvr.txtTargetAngle;
	cvr.txtPos = cvr.txtTargetPos;
	cvr.txtColor = cvr.txtTargetColor;
}

bool CCoverFlow::_sortByPlayCount(CItem item1, CItem item2)
{
	return (item1.playcount == item2.playcount) ? _sortByAlpha(item1, item2) : item1.playcount > item2.playcount;
}

bool CCoverFlow::_sortByLastPlayed(CItem item1, CItem item2)
{
	return (item1.lastPlayed == item2.lastPlayed) ? _sortByPlayCount(item1, item2) : item1.lastPlayed > item2.lastPlayed;
}

bool CCoverFlow::_sortByGameID(CItem item1, CItem item2)
{
	u32 s = min(strlen(item1.hdr->id), strlen(item2.hdr->id));
	for(u32 k = 0; k < s; ++k)
	{
		if(toupper(item1.hdr->id[k]) > toupper(item2.hdr->id[k]))
			return false;
		else if(toupper(item1.hdr->id[k]) < toupper(item2.hdr->id[k]))
			return true;
	}

	return strlen(item1.hdr->id) < strlen(item2.hdr->id);
}

bool CCoverFlow::_sortByAlpha(CItem item1, CItem item2)
{
	const wchar_t *first = item1.hdr->title;
	const wchar_t *second = item2.hdr->title;
	return wchar_cmp(first, second, wcslen(first), wcslen(second));
}

bool CCoverFlow::_sortByPlayers(CItem item1, CItem item2)
{
	if(item1.hdr->players == item2.hdr->players) return _sortByAlpha(item1, item2);
	return item1.hdr->players < item2.hdr->players;
}

bool CCoverFlow::_sortByWifiPlayers(CItem item1, CItem item2)
{
	if(item1.hdr->wifi == item2.hdr->wifi) return _sortByAlpha(item1, item2);
	return item1.hdr->wifi < item2.hdr->wifi;
}

bool CCoverFlow::_sortByBtnNumbers(CItem item1, CItem item2)
{
	if(item1.hdr->settings[0] == item2.hdr->settings[0]) return _sortByAlpha(item1, item2);
	return item1.hdr->settings[0] < item2.hdr->settings[0];
}

bool CCoverFlow::start(const string &m_imgsDir)
{
	if (m_items.empty()) return true;

	/* sort coverflow items list based on sort type */
	if (m_sorting == SORT_ALPHA)
		sort(m_items.begin(), m_items.end(), CCoverFlow::_sortByAlpha);
	else if (m_sorting == SORT_PLAYCOUNT)
		sort(m_items.begin(), m_items.end(), CCoverFlow::_sortByPlayCount);
	else if (m_sorting == SORT_LASTPLAYED)
		sort(m_items.begin(), m_items.end(), CCoverFlow::_sortByLastPlayed);
	else if (m_sorting == SORT_GAMEID)
		sort(m_items.begin(), m_items.end(), CCoverFlow::_sortByGameID);
	else if (m_sorting == SORT_PLAYERS)
		sort(m_items.begin(), m_items.end(), CCoverFlow::_sortByPlayers);
	else if (m_sorting == SORT_WIFIPLAYERS)
		sort(m_items.begin(), m_items.end(), CCoverFlow::_sortByWifiPlayers);
	else if (m_sorting == SORT_BTN_NUMBERS)
		sort(m_items.begin(), m_items.end(), CCoverFlow::_sortByBtnNumbers);

	/* load the colored skin/spine images if not already done */
	if(!m_dvdskin_loaded)
	{
		if(TexHandle.fromImageFile(m_dvdSkin, fmt("%s/dvdskin.png", m_imgsDir.c_str())) != TE_OK)
			return false;
		if(TexHandle.fromImageFile(m_dvdSkin_Red, fmt("%s/dvdskin_red.png", m_imgsDir.c_str())) != TE_OK)
			return false;
		if(TexHandle.fromImageFile(m_dvdSkin_Black, fmt("%s/dvdskin_black.png", m_imgsDir.c_str())) != TE_OK)
			return false;
		if(TexHandle.fromImageFile(m_dvdSkin_Yellow, fmt("%s/dvdskin_yellow.png", m_imgsDir.c_str())) != TE_OK)
			return false;
		if(TexHandle.fromImageFile(m_dvdSkin_GreenOne, fmt("%s/dvdskin_greenone.png", m_imgsDir.c_str())) != TE_OK)
			return false;
		if(TexHandle.fromImageFile(m_dvdSkin_GreenTwo, fmt("%s/dvdskin_greentwo.png", m_imgsDir.c_str())) != TE_OK)
			return false;
		if(TexHandle.fromImageFile(m_dvdSkin_Clear, fmt("%s/dvdskin_clear.png", m_imgsDir.c_str())) != TE_OK)
			return false;
		m_dvdskin_loaded = true;
	}
	/* load the no pic or loading cover images if not already done */
	if(!m_defcovers_loaded)
	{
		if(m_pngLoadCover.empty() || TexHandle.fromImageFile(m_boxLoadingTexture, m_pngLoadCover.c_str(), GX_TF_CMPR, 32, 512) != TE_OK)
		{
			if(TexHandle.fromImageFile(m_boxLoadingTexture, fmt("%s/loading.png", m_imgsDir.c_str()), GX_TF_CMPR, 32, 512) != TE_OK)
				return false;
		}
		if(m_pngNoCover.empty() || TexHandle.fromImageFile(m_boxNoCoverTexture, m_pngNoCover.c_str(), GX_TF_CMPR, 32, 512) != TE_OK)
		{
			if(TexHandle.fromImageFile(m_boxNoCoverTexture, fmt("%s/nopic.png", m_imgsDir.c_str()), GX_TF_CMPR, 32, 512) != TE_OK)
				return false;
		}
		if(m_pngLoadCoverFlat.empty() || TexHandle.fromImageFile(m_flatLoadingTexture, m_pngLoadCoverFlat.c_str(), GX_TF_CMPR, 32, 512) != TE_OK)
		{
			if(TexHandle.fromImageFile(m_flatLoadingTexture, fmt("%s/flatloading.png", m_imgsDir.c_str()), GX_TF_CMPR, 32, 512) != TE_OK)
				return false;
		}
		if(m_pngNoCoverFlat.empty() || TexHandle.fromImageFile(m_flatNoCoverTexture, m_pngNoCoverFlat.c_str(), GX_TF_CMPR, 32, 512) != TE_OK)
		{
			if(TexHandle.fromImageFile(m_flatNoCoverTexture, fmt("%s/flatnopic.png", m_imgsDir.c_str()), GX_TF_CMPR, 32, 512) != TE_OK)
				return false;
		}
		m_defcovers_loaded = true;
	}

	/* allocate enough memory for the covers list (m_covers) based on rows * columns (m_range) */
	if(m_covers != NULL)
		MEM2_free(m_covers);
	m_covers = NULL;
	if(m_range > 0)
	{
		m_covers = (CCover*)MEM2_alloc(sizeof(CCover) * m_range);
		for(size_t i = 0; i < m_range; ++i)
			m_covers[i] = *(new(m_covers+i) CCover);
	}
	m_jump = 0;
	m_selected = false;
	m_moved = true;
	return true;
}

void CCoverFlow::up(void)
{
	if (m_covers == NULL) return;
	if (m_jump != 0) return;

	LockMutex lock(m_mutex);
	_left(m_minDelay, 1);
}

void CCoverFlow::down(void)
{
	if (m_covers == NULL) return;
	if (m_jump != 0) return;

	LockMutex lock(m_mutex);
	_right(m_minDelay, 1);
}

void CCoverFlow::left(void)
{
	if (m_covers == NULL) return;
	if (m_jump != 0) return;

	LockMutex lock(m_mutex);
	_left(m_minDelay, m_rows >= 3 ? m_rows - 2 : 1);
}

void CCoverFlow::right(void)
{
	if (m_covers == NULL) return;
	if (m_jump != 0) return;

	LockMutex lock(m_mutex);
	_right(m_minDelay, m_rows >= 3 ? m_rows - 2 : 1);
}

void CCoverFlow::_left(int repeatDelay, u32 step)
{
	int prev, arrStep;

	if (m_delay > 0) return;

	m_moved = true;
	m_delay = repeatDelay;
	m_covers[m_range / 2].angle += _coverMovesA();
	m_covers[m_range / 2].pos += _coverMovesP();
	if (m_rows >= 3)
	{
		prev = m_covers[m_range / 2].index;
		// Shift the array to the right
		arrStep = step % (m_rows - 2) + (step / (m_rows - 2)) * m_rows;
		for (int i = (int)m_range - 1; i >= arrStep; --i)
			m_covers[i] = m_covers[i - arrStep];
		_loadAllCovers(loopNum((int)prev - step, m_items.size()));
		_updateAllTargets();
		if (arrStep % m_rows != 0)
			for (u32 i = 0; i < m_columns; ++i)
				_instantTarget(i * m_rows);
		if (arrStep / m_rows != 0)
			for (u32 i = 0; i < m_rows; ++i)
				_instantTarget(i);
	}
	else
	{
		prev = m_covers[0].index;
		// Shift the array to the right
		for (int i = (int)m_range - 1; i > 0; --i)
			m_covers[i] = m_covers[i - 1];
		_loadCover(0, loopNum(prev - step, m_items.size()));
		_updateAllTargets();
		_instantTarget(0);
	}
	_playSound(m_flipSound);
	m_covers[m_range / 2].angle -= _coverMovesA();
	m_covers[m_range / 2].pos -= _coverMovesP();
}

void CCoverFlow::_right(int repeatDelay, u32 step)
{
	int prev, arrStep;

	if (m_delay > 0) return;

	m_moved = true;
	m_delay = repeatDelay;
	m_covers[m_range / 2].angle += _coverMovesA();
	m_covers[m_range / 2].pos += _coverMovesP();
	if (m_rows >= 3)
	{
		prev = m_covers[m_range / 2].index;
		// Shift the array to the left
		arrStep = step % (m_rows - 2) + (step / (m_rows - 2)) * m_rows;
		for (u32 i = 0; i < m_range - arrStep; ++i)
			m_covers[i] = m_covers[i + arrStep];
		_loadAllCovers(loopNum(prev + step, m_items.size()));
		_updateAllTargets();
		if (arrStep % m_rows != 0)
			for (u32 i = 0; i < m_columns; ++i)
				_instantTarget(i * m_rows + m_rows - 1);
		if (arrStep / m_rows != 0)
			for (u32 i = 0; i < m_rows; ++i)
				_instantTarget(i + (m_columns - 1) * m_rows);
	}
	else
	{
		prev = m_covers[m_range - 1].index;
		// Shift the array to the left
		for (u32 i = 0; i < m_range - 1; ++i)
			m_covers[i] = m_covers[i + 1];
		_loadCover(m_range - 1, loopNum(prev + step, m_items.size()));
		_updateAllTargets();
		_instantTarget(m_range - 1);
	}
	_playSound(m_flipSound);
	m_covers[m_range / 2].angle -= _coverMovesA();
	m_covers[m_range / 2].pos -= _coverMovesP();
}

void CCoverFlow::mouse(int chan, int x, int y)
{
	if (m_covers == NULL) return;

	int m = m_mouse[chan];
	if (x < 0 || y < 0)
		m_mouse[chan] = -1;
	else
	{
		m_vid.prepareStencil();
		_draw(CCoverFlow::CFDR_STENCIL, false, false);
		m_vid.renderStencil();
		m_mouse[chan] = m_vid.stencilVal(x, y) - 1;
	}
	if (m != m_mouse[chan])
	{
		if ((u32)m_mouse[chan] < m_range && m_mouse[chan] >= 0)
			_playSound(m_hoverSound);
		_updateAllTargets();
	}
}

bool CCoverFlow::mouseOver(int x, int y)
{
	if (m_covers == NULL) return false;

	m_vid.prepareStencil();
	_draw(CCoverFlow::CFDR_STENCIL, false, false);
	m_vid.renderStencil();
	m_vid.prepareStencil();
	_draw(CCoverFlow::CFDR_STENCIL, false, false);
	m_vid.renderStencil();
	return m_vid.stencilVal(x, y) == (int)m_range / 2 + 1;
}

void CCoverFlow::setSelected(int i)
{
	LockMutex lock(m_mutex);
	m_moved = true;
	_loadAllCovers(i);
	_updateAllTargets(true);
	select();
}

void CCoverFlow::pageUp(void)
{
	if (m_covers == NULL) return;

	int n, j;
	if (m_rows >= 3)
	{
		j = m_jump - max(1, (int)m_columns - 2);
		if (m_jump != 0)
		{
			n = ((int)m_items.size() - 1) / (m_rows - 2) + 1;
			j = j < 0 ? -(-j % n) : j % n;
		}
		j *= (int)m_rows - 2;
	}
	else
	{
		j = m_jump - max(1, (int)m_range / 2);
		if (m_jump != 0)
		{
			n = (int)m_items.size();
			j = j < 0 ? -(-j % n) : j % n;
		}
	}
	_setJump(j);
}

void CCoverFlow::pageDown(void)
{
	if (m_covers == NULL) return;

	int n, j;
	if (m_rows >= 3)
	{
		j = m_jump + max(1, (int)m_columns - 2);
		if (m_jump != 0)
		{
			n = ((int)m_items.size() - 1) / (m_rows - 2) + 1;
			j = j < 0 ? -(-j % n) : j % n;
		}
		j *= (int)m_rows - 2;
	}
	else
	{
		j = m_jump + max(1, (int)m_range / 2);
		if (m_jump != 0)
		{
			n = (int)m_items.size();
			j = j < 0 ? -(-j % n) : j % n;
		}
	}
	_setJump(j);
}

void CCoverFlow::flip(bool force, bool f)
{
	if (m_covers == NULL || !m_selected) return;
	LockMutex lock(m_mutex);

	CCoverFlow::CCover &cvr = m_covers[m_range / 2];
	if (!force)
		f = m_loSelected.centerAngle == cvr.targetAngle && m_loSelected.centerPos == cvr.targetPos && m_loSelected.centerScale == cvr.targetScale;
	if (f)
	{
		cvr.targetPos = m_loSelected.centerPos + m_flipCoverPos;
		cvr.targetAngle = m_loSelected.centerAngle + m_flipCoverAngle;
		cvr.targetScale = m_loSelected.centerScale * m_flipCoverScale;
		cvr.txtTargetColor = m_flipCoverPos == Vector3D(0.f, 0.f, 0.f) ? 0xFF : 0;
	}
	else
	{
		cvr.targetPos = m_loSelected.centerPos;
		cvr.targetAngle = m_loSelected.centerAngle;
		cvr.targetScale = m_loSelected.centerScale;
		cvr.txtTargetColor = 0xFF;
	}
}

bool CCoverFlow::_invisibleCover(u32 x, u32 y)
{
	return (m_rows > 1 && (y == 0 || y == m_rows - 1)) || x == 0 || x == m_columns - 1;
}

void CCoverFlow::_loadAllCovers(int i)
{
	int r = (int)m_rows;
	int c = (int)m_columns;
	int s = (int)m_items.size();

	if (m_rows >= 3)
		for (int j = 0; j < r; ++j)
			for (int k = 0; k < c; ++k)
				_loadCover(j + k * r, loopNum(i + (j - r / 2) + (k - c / 2) * (r - 2), s));
	else
		for (int k = 0; k < (int)m_range; ++k)
			_loadCover(k, loopNum(i + k - (int)m_range / 2, s));
}

void CCoverFlow::_setJump(int j)
{
	_loadAllCovers(_currentPos());
	m_jump = j;
	if (m_jump == 0)
		_updateAllTargets();
}

void CCoverFlow::_completeJump(void)
{
	if (m_rows >= 3)
		_loadAllCovers((int)m_covers[m_range / 2].index + m_jump);
	else
	{
		if (m_jump < 0)
			_loadAllCovers((int)m_covers[0].index + m_jump + (int)m_range / 2);
		else
			_loadAllCovers((int)m_covers[m_range - 1].index + m_jump - ((int)m_range - 1) / 2);
	}
}

void CCoverFlow::nextLetter(wchar_t *c)
{
	if (m_covers == NULL || m_sorting == SORT_BTN_NUMBERS)
	{
		c[0] = L'\0';
		return;
	}

	if (m_sorting == SORT_WIFIPLAYERS || m_sorting == SORT_PLAYERS)
		return nextPlayers(m_sorting == SORT_WIFIPLAYERS, c);

	if (m_sorting == SORT_GAMEID)
		return nextID(c);

	LockMutex lock(m_mutex);

	u32 i, j = 0, k = 0, n = m_items.size();

	_completeJump();
	u32 curPos = _currentPos();

	while (!iswalnum(m_items[curPos].hdr->title[j]) && m_items[curPos].hdr->title[j+1] != L'\0') j++;
	c[0] = upperCaseWChar(m_items[curPos].hdr->title[j]);

	for (i = 1; i < n; ++i)
	{
		k = 0;
		while (!iswalnum(m_items[loopNum(curPos + i, n)].hdr->title[k]) && m_items[loopNum(curPos + i, n)].hdr->title[k+1] != L'\0') k++;
		if (upperCaseWChar(m_items[loopNum(curPos + i, n)].hdr->title[k]) != c[0])
			break;
	}
	if (i < n)
	{
		_setJump(i);
		c[0] = upperCaseWChar(m_items[loopNum(curPos + i, n)].hdr->title[k]);
	}
	_updateAllTargets();
}

void CCoverFlow::prevLetter(wchar_t *c)
{
	if (m_covers == NULL || m_sorting == SORT_BTN_NUMBERS)
	{
		c[0] = L'\0';
		return;
	}

	if (m_sorting == SORT_WIFIPLAYERS || m_sorting == SORT_PLAYERS)
		return prevPlayers(m_sorting == SORT_WIFIPLAYERS, c);

	if (m_sorting == SORT_GAMEID)
		return prevID(c);

	LockMutex lock(m_mutex);
	u32 i, j = 0, k = 0, n = m_items.size();

	_completeJump();
	u32 curPos = _currentPos();
	
	while (!iswalnum(m_items[curPos].hdr->title[j]) && m_items[curPos].hdr->title[j+1] != L'\0') j++;
	c[0] = upperCaseWChar(m_items[curPos].hdr->title[j]);

	for (i = 1; i < n; ++i)
	{
		k = 0;
		while (!iswalnum(m_items[loopNum(curPos - i, n)].hdr->title[k]) && m_items[loopNum(curPos - i, n)].hdr->title[k+1] != L'\0') k++;
		if (upperCaseWChar(m_items[loopNum(curPos - i, n)].hdr->title[k]) != c[0])
		{
			c[0] = upperCaseWChar(m_items[loopNum(curPos - i, n)].hdr->title[k]);
			while(i < n)
			{
				++i;
				k = 0;
				while (!iswalnum(m_items[loopNum(curPos - i, n)].hdr->title[k]) && m_items[loopNum(curPos - i, n)].hdr->title[k+1] != L'\0') k++;
				if(upperCaseWChar(m_items[loopNum(curPos - i, n)].hdr->title[k]) != c[0])
					break;
			}
			i--;
			break;
		}
	}

	if (i < n)
	{
		_setJump(-i);
		k = 0;
		while (!iswalnum(m_items[loopNum(curPos - i, n)].hdr->title[k]) && m_items[loopNum(curPos - i, n)].hdr->title[k+1] != L'\0') k++;
		c[0] = upperCaseWChar(m_items[loopNum(curPos - i, n)].hdr->title[k]);
	}

	_updateAllTargets();
}

void CCoverFlow::nextPlayers(bool wifi, wchar_t *c)
{
	LockMutex lock(m_mutex);
	u32 i, n = m_items.size();

	_completeJump();
	u32 curPos = _currentPos();
	int players = wifi ? m_items[curPos].hdr->wifi : m_items[curPos].hdr->players;

	for (i = 1; i < n; ++i)
		if ((wifi ? m_items[loopNum(curPos + i, n)].hdr->wifi : m_items[loopNum(curPos + i, n)].hdr->players) != players)
			break;

	if (i < n)
	{
		_setJump(i);
		players = wifi ? m_items[loopNum(curPos + i, n)].hdr->wifi : m_items[loopNum(curPos + i, n)].hdr->players;
	}

	char p[4] = {0 ,0 ,0 ,0};
	sprintf(p, "%d", players);
	mbstowcs(c, p, strlen(p));

	_updateAllTargets();
}

void CCoverFlow::prevPlayers(bool wifi, wchar_t *c)
{
	LockMutex lock(m_mutex);
	u32 i, n = m_items.size();

	_completeJump();
	u32 curPos = _currentPos();
	int players = wifi ? m_items[curPos].hdr->wifi : m_items[curPos].hdr->players;

	for (i = 1; i < n; ++i)
		if ((wifi ? m_items[loopNum(curPos - i, n)].hdr->wifi : m_items[loopNum(curPos - i, n)].hdr->players) != players)
		{
			players = wifi ? m_items[loopNum(curPos - i, n)].hdr->wifi : m_items[loopNum(curPos - i, n)].hdr->players;
			while(i < n && (wifi ? m_items[loopNum(curPos - i, n)].hdr->wifi : m_items[loopNum(curPos - i, n)].hdr->players) == players) ++i;
			i--;
			break;
		}

	if (i < n)
	{
		_setJump(-i);
		players = wifi ? m_items[loopNum(curPos - i, n)].hdr->wifi : m_items[loopNum(curPos - i, n)].hdr->players;
	}

	char p[4] = {0 ,0 ,0 ,0};
	sprintf(p, "%d", players);
	mbstowcs(c, p, strlen(p));

	_updateAllTargets();
}

void CCoverFlow::nextID(wchar_t *c)
{
	LockMutex lock(m_mutex);
	u32 i, n = m_items.size();

	_completeJump();
	u32 curPos = _currentPos();
	char *system = m_items[curPos].hdr->id;

	for (i = 1; i < n; ++i)
		if (m_items[loopNum(curPos + i, n)].hdr->id[0] != system[0])
			break;

	if (i < n)
	{
		_setJump(i);
		system = m_items[loopNum(curPos + i, n)].hdr->id;
	}

	system[1] = '\0';
	mbstowcs(c, system, 1);

	_updateAllTargets();
}

void CCoverFlow::prevID(wchar_t *c)
{
	LockMutex lock(m_mutex);
	u32 i, n = m_items.size();

	_completeJump();
	u32 curPos = _currentPos();
	char *system = m_items[curPos].hdr->id;

	for(i = 1; i < n; ++i)
		if(m_items[loopNum(curPos - i, n)].hdr->id[0] != system[0])
		{
			system = m_items[loopNum(curPos - i, n)].hdr->id;
			while(i < n && m_items[loopNum(curPos - i, n)].hdr->id[0] == system[0]) ++i;
			i--;
			break;
		}

	if (i < n)
	{
		_setJump(-i);
		system = m_items[loopNum(curPos - i, n)].hdr->id;
	}

	system[1] = '\0';
	mbstowcs(c, system, 1);

	_updateAllTargets();
}

void CCoverFlow::_coverTick(int i)
{
	float speed = m_selected ? m_selected_speed : m_normal_speed;
	Vector3D posDist(m_covers[i].targetPos - m_covers[i].pos);

	if (posDist.sqNorm() < 0.5f)
		speed *= 0.5f;
	m_covers[i].angle += (m_covers[i].targetAngle - m_covers[i].angle) * speed;
	m_covers[i].pos += posDist * speed;
	m_covers[i].scale += (m_covers[i].targetScale - m_covers[i].scale) * speed;
	if (m_covers[i].color != m_covers[i].targetColor)
	{
		CColor c(m_covers[i].color);
		m_covers[i].color = CColor::interpolate(c, m_covers[i].targetColor, 0x20);
		if (m_covers[i].color == c)	// If the interpolation doesn't do anything because of numerical approximation, force the target color
			m_covers[i].color = m_covers[i].targetColor;
	}
	if (m_covers[i].shadowColor != m_covers[i].targetShadowColor)
	{
		CColor c(m_covers[i].shadowColor);
		m_covers[i].shadowColor = CColor::interpolate(c, m_covers[i].targetShadowColor, 0x20);
		if (m_covers[i].shadowColor == c)	// If the interpolation doesn't do anything because of numerical approximation, force the target color
			m_covers[i].shadowColor = m_covers[i].targetShadowColor;
	}
	m_covers[i].txtAngle += (m_covers[i].txtTargetAngle - m_covers[i].txtAngle) * speed;
	m_covers[i].txtPos += (m_covers[i].txtTargetPos - m_covers[i].txtPos) * speed;
	int colorDist = (int)m_covers[i].txtTargetColor - (int)m_covers[i].txtColor;
	m_covers[i].txtColor += abs(colorDist) >= 8 ? (u8)(colorDist / 8) : (u8)colorDist;
	m_covers[i].title.tick();
}

void CCoverFlow::_unselect(void)
{
	if (m_selected)
	{
		m_cameraPos += _cameraMoves();
		m_covers[m_range / 2].angle += _coverMovesA();
		m_covers[m_range / 2].pos += _coverMovesP();
		m_selected = false;
		m_covers[m_range / 2].angle -= _coverMovesA();
		m_covers[m_range / 2].pos -= _coverMovesP();
		m_cameraPos -= _cameraMoves();
		m_targetCameraPos = m_loNormal.camera;
		m_targetCameraAim = m_loNormal.cameraAim;
	}
}

void CCoverFlow::_jump(void)
{
	int step, delay = 2;

	if (m_rows >= 3)
		step = (u32)abs(m_jump) <= (m_rows - 2) / 2 ? 1 : m_rows - 2;
	else
		// Cheat and skip covers we wouldn't see anyway
		// This means the jump shouldn't be modified before it's done completely
		step = (u32)abs(m_jump) > m_range + 1 ? abs(m_jump) - (m_range + 1) : 1;
	if (m_jump < 0)
	{
		_left(delay, step);
		m_jump += step;
	}
	else if (m_jump > 0)
	{
		_right(delay, step);
		m_jump -= step;
	}
}

void CCoverFlow::tick(void)
{
	if (m_covers == NULL) return;

	LockMutex lock(m_mutex);
	++m_tickCount;
	if (m_delay > 0)
		--m_delay;
	else
		_jump();
	for (u32 i = 0; i < m_range; ++i)
		_coverTick(i);
	m_cameraPos += (m_targetCameraPos - m_cameraPos) * 0.2f;
	m_cameraAim += (m_targetCameraAim - m_cameraAim) * 0.2f;
}

struct SWFCHeader
{
	u8 newFmt : 1;	// Was 0 in beta
	u8 full : 1;
	u8 cmpr : 1;
	u8 zipped : 1;
	u8 backCover : 1;
	u16 width;
	u16 height;
	u8 maxLOD;
	u16 backWidth;
	u16 backHeight;
	u8 backMaxLOD;
public:
	u32 getWidth(void) const { return width * 4; }
	u32 getHeight(void) const { return height * 4; }
	u32 getBackWidth(void) const { return backWidth * 4; }
	u32 getBackHeight(void) const { return backHeight * 4; }
	SWFCHeader(void)
	{
		memset(this, 0, sizeof *this);
	}
	SWFCHeader(const TexData &tex, bool f, bool z, const TexData &backTex = TexData())
	{
		newFmt = 1;
		full = f ? 1 : 0;
		cmpr = tex.format == GX_TF_CMPR ? 1 : 0;
		zipped = z ? 1 : 0;
		width = tex.width / 4;
		height = tex.height / 4;
		maxLOD = tex.maxLOD;
		backCover = !!backTex.data ? 1 : 0;
		backWidth = backTex.width / 4;
		backHeight = backTex.height / 4;
		backMaxLOD = backTex.maxLOD;
	}
};

bool CCoverFlow::cacheCoverFile(const char *wfcPath, const char *coverPath, bool full)
{
	TexData tex;
	tex.thread = true;// lets TexHandle know this texture is a cover image and in case its a homebrew icon.png
	m_renderingTex = &tex;// only used if cover has alpha transparency - homebrew icon.png and sourceflow smallbox
	u8 textureFmt = m_compressTextures ? GX_TF_CMPR : GX_TF_RGB565;// always GX_TF_CMPR
	if(TexHandle.fromImageFile(tex, coverPath, textureFmt, 32) != TE_OK)
	{
		m_renderingTex = NULL;
		return false;
	}
	m_renderingTex = NULL;
	tex.thread = false;
	u32 bufSize = fixGX_GetTexBufferSize(tex.width, tex.height, tex.format, tex.maxLOD > 0 ? GX_TRUE : GX_FALSE, tex.maxLOD);
	if(tex.data != NULL)
	{
		FILE *file = fopen(wfcPath, "wb");
		if(file != NULL)
		{
			SWFCHeader header(tex, full, false);
			fwrite(&header, 1, sizeof(header), file);
			fwrite(tex.data, 1, bufSize, file);
			fclose(file);
		}
	}
	TexHandle.Cleanup(tex);
	return true;
}

bool CCoverFlow::cacheCoverBuffer(const char *wfcPath, const u8 *png, bool full)
{
	TexData tex;
	u8 textureFmt = m_compressTextures ? GX_TF_CMPR : GX_TF_RGB565;// always GX_TF_CMPR
	if(TexHandle.fromPNG(tex, png, textureFmt, 32) != TE_OK)
		return false;

	u32 bufSize = fixGX_GetTexBufferSize(tex.width, tex.height, tex.format, tex.maxLOD > 0 ? GX_TRUE : GX_FALSE, tex.maxLOD);
	if(tex.data != NULL)
	{
		FILE *file = fopen(wfcPath, "wb");
		if(file != NULL)
		{
			SWFCHeader header(tex, full, false);
			fwrite(&header, 1, sizeof(header), file);
			fwrite(tex.data, 1, bufSize, file);
			fclose(file);
		}
	}
	TexHandle.Cleanup(tex);
	return true;
}

bool CCoverFlow::fullCoverCached(const char *wfcPath)
{
	bool found = false;

	FILE *file = fopen(wfcPath, "rb");
	if(file != NULL)
	{
		SWFCHeader header;
		found = fread(&header, 1, sizeof header, file) == sizeof header
			&& header.full != 0 && m_compressTextures == (header.cmpr != 0)
			&& header.getWidth() >= 8 && header.getHeight() >= 8
			&& header.getWidth() <= 1090 && header.getHeight() <= 1090;
		fclose(file);
	}
	return found;
}

bool CCoverFlow::_calcTexLQLOD(TexData &tex)
{
	bool done = false;

	while (tex.width > 512 && tex.height > 512 && tex.maxLOD > 0)
	{
		--tex.maxLOD;
		tex.width >>= 1;// divide by 2
		tex.height >>= 1;// divide by 2
		done = true;
	}
	return done;
}

void CCoverFlow::_dropHQLOD(int i)
{
	if ((u32)i >= m_items.size()) return;

	LockMutex lock(m_mutex);

	const TexData &prevTex = m_items[i].texture;
	if(prevTex.data == NULL)
		return;
	TexData newTex;

	newTex.maxLOD = prevTex.maxLOD;
	newTex.width = prevTex.width;
	newTex.height = prevTex.height;
	newTex.format = prevTex.format;

	if (!CCoverFlow::_calcTexLQLOD(newTex)) return;

	u32 prevTexLen = fixGX_GetTexBufferSize(prevTex.width, prevTex.height, prevTex.format, prevTex.maxLOD > 0 ? GX_TRUE : GX_FALSE, prevTex.maxLOD);
	u32 newTexLen = fixGX_GetTexBufferSize(newTex.width, newTex.height, newTex.format, newTex.maxLOD > 0 ? GX_TRUE : GX_FALSE, newTex.maxLOD);
	newTex.data = (u8*)MEM2_alloc(newTexLen);
	if(newTex.data == NULL)
		return;
	memcpy(newTex.data, prevTex.data + (prevTexLen - newTexLen), newTexLen);
	DCFlushRange(newTex.data, newTexLen);
	TexHandle.Cleanup(m_items[i].texture);
	m_items[i].texture = newTex;
}

const char *CCoverFlow::getFilenameId(const dir_discHdr *curHdr)
{
	if(NoGameID(curHdr->type))
	{
		if(strrchr(curHdr->path, '/') != NULL)
			return strrchr(curHdr->path, '/') + 1;// app folder or rom name.ext
		else
			return curHdr->path;// title for scummvm
	}
	return curHdr->id;// ID for Wii, GC, & Channels
}

CCoverFlow::CLRet CCoverFlow::_loadCoverTex(u32 i, bool box, bool hq, bool blankBoxCover)
{
	if(!m_loadingCovers) 
		return CL_ERROR;

	if(box && m_smallBox)// prevent smallbox from loading full box cover
		return CL_ERROR;
		
	if(blankBoxCover && m_items[i].hdr->type == TYPE_SOURCE)// blank covers not used for sourceflow
		return CL_ERROR;
		
	bool allocFailed = false;

	/* try to find the wfc texture file in the cache folder */
	if(!m_cachePath.empty())
	{
		char wfcTitle[128];
		wfcTitle[127] = '\0';
		const char *wfcCoverDir = NULL;
		char *full_path = (char*)MEM2_alloc(MAX_FAT_PATH+1);
		if(full_path == NULL)
			return CL_NOMEM;
		memset(full_path, 0, MAX_FAT_PATH+1);
		
		/* get title for wfc file */
		if(blankBoxCover)
		{
			const char *blankCoverPath = mainMenu.getBlankCoverPath(m_items[i].hdr);
			if(blankCoverPath != NULL && strrchr(blankCoverPath, '/') != NULL)
				strncpy(wfcTitle, strrchr(blankCoverPath, '/') + 1, sizeof(wfcTitle) - 1);
			else
				return CL_ERROR;
		}
		else
			strncpy(wfcTitle, fmt("%s", getFilenameId(m_items[i].hdr)), sizeof(wfcTitle) - 1);
			
		/* get coverfolder for plugins, sourceflow, and homebrew */
		if(m_items[i].hdr->type == TYPE_PLUGIN)
			wfcCoverDir = m_plugin.GetCoverFolderName(m_items[i].hdr->settings[0]);
		if(m_items[i].hdr->type == TYPE_SOURCE)
			wfcCoverDir = "sourceflow";
		if(m_items[i].hdr->type == TYPE_HOMEBREW)
			wfcCoverDir = "homebrew";
			
		/* set full path of wfc file */
		if(wfcCoverDir != NULL)
		{
			if(m_smallBox)
				strncpy(full_path, fmt("%s/%s/%s_small.wfc", m_cachePath.c_str(), wfcCoverDir, wfcTitle), MAX_FAT_PATH);
			else
			{
				strncpy(full_path, fmt("%s/%s/%s.wfc", m_cachePath.c_str(), wfcCoverDir, wfcTitle), MAX_FAT_PATH);
				/*if(!fsop_FileExist(full_path))
				{
					if(strrchr(wfcTitle, '.') != NULL)
					{
						*strrchr(wfcTitle, '.') = '\0';
						strncpy(full_path, fmt("%s/%s/%s.wfc", m_cachePath.c_str(), wfcCoverDir, wfcTitle), MAX_FAT_PATH);
					}
				}*/
			}
		}
		else
		{
			if(m_smallBox)
				strncpy(full_path, fmt("%s/%s_small.wfc", m_cachePath.c_str(), wfcTitle), MAX_FAT_PATH);
			else
				strncpy(full_path, fmt("%s/%s.wfc", m_cachePath.c_str(), wfcTitle), MAX_FAT_PATH);
		}
		DCFlushRange(full_path, MAX_FAT_PATH+1);
		
		/* load wfc file */
		FILE *fp;
		fp = fopen(full_path, "rb");
		free(full_path);

		if(fp != NULL)//if wfc chache file is found
		{
			bool success = false;
			if(fseek(fp, 0, SEEK_END) != 0)
			{
				fclose(fp);
				return CL_ERROR;
			}
			u32 fileSize = ftell(fp);
			if(fseek(fp, 0, SEEK_SET) != 0)
			{
				fclose(fp);
				return CL_ERROR;
			}
			
			SWFCHeader header;
			if(fileSize > sizeof(header))
			{
				if(fread(&header, 1, sizeof(header), fp) != sizeof(header))
				{
					fclose(fp);
					return CL_ERROR;
				}
				DCFlushRange(&header, sizeof(header));
				//make sure wfc cache file matches what we want
				if(header.newFmt == 1 && (header.full != 0) == box && (header.cmpr != 0) == m_compressTextures)
				{
					TexData tex;
					tex.format = header.cmpr != 0 ? GX_TF_CMPR : GX_TF_RGB565;
					tex.width = header.getWidth();
					tex.height = header.getHeight();
					tex.maxLOD = header.maxLOD;

					/* note bufSize and texLen will be the same if cover is HQ */
					u32 bufSize = fixGX_GetTexBufferSize(tex.width, tex.height, tex.format, tex.maxLOD > 0 ? GX_TRUE : GX_FALSE, tex.maxLOD);
					if(!hq)
						CCoverFlow::_calcTexLQLOD(tex);
					u32 texLen = fixGX_GetTexBufferSize(tex.width, tex.height, tex.format, tex.maxLOD > 0 ? GX_TRUE : GX_FALSE, tex.maxLOD);

					tex.data = (u8*)MEM2_alloc(texLen);
					if(tex.data == NULL)
						allocFailed = true;
					else
					{
						/* if not HQ cover then skip (bufSize - texLen) texture data after header */
						fseek(fp, sizeof(header) + (bufSize - texLen), SEEK_SET);
						if(fread(tex.data, 1, texLen, fp) == texLen)
						{
							DCFlushRange(tex.data, texLen);
							LockMutex lock(m_mutex);
							TexHandle.Cleanup(m_items[i].texture);
							m_items[i].texture = tex;
							m_items[i].state = STATE_Ready;
							m_items[i].boxTexture = header.full != 0;
							success = true;
						}
					}
					if(!success && tex.data != NULL)
					{
						free(tex.data);
						tex.data = NULL;
					}
				}
			}
			fclose(fp);
			if(success)
				return CL_OK;
		}
	}
	if(allocFailed)
		return CL_NOMEM;

	return CL_ERROR;
}

void * CCoverFlow::_coverLoader(void *obj)
{
	CCoverFlow *cf = static_cast<CCoverFlow *>(obj);
	cf->m_coverThrdBusy = true;
	CLRet ret;
	u32 firstItem;
	bool update;
	u32 i, j;
	bool hq_req = cf->m_useHQcover;
	bool cur_pos_hq = false;
	u32 bufferSize = min(cf->m_numBufCovers * max(2u, cf->m_rows), 80u);

	while(cf->m_loadingCovers)
	{
		update = cf->m_moved;
		cf->m_moved = false;
		firstItem = cf->m_covers[cf->m_range / 2].index;
		for(j = cf->m_items.size(); j >= bufferSize && !cf->m_moved && update; --j)
		{
			i = loopNum((j & 1) ? firstItem - (j + 1) / 2 : firstItem + j / 2, cf->m_items.size());
			if(cf->m_items[i].state != STATE_Loading)
			{
				LWP_MutexLock(cf->m_mutex);
				TexHandle.Cleanup(cf->m_items[i].texture);
				cf->m_items[i].state = STATE_Loading;
				LWP_MutexUnlock(cf->m_mutex);
			}
		}
		ret = CL_OK;
		bool hq_done = false;
		/* we try 3 passes to get the full cover. because the cover loader randomly skips the wfc file for unknown reasons */
		/* first time is full cover only */
		/* second time is full then front */
		/* third is full then front then custom blank cover */
		for(u8 k = 1; k < 4 && !cf->m_moved && update && ret != CL_NOMEM; k++)
		{
			for(j = 0; j <= bufferSize && !cf->m_moved && update && ret != CL_NOMEM; ++j)
			{
				i = loopNum((j & 1) ? firstItem - (j + 1) / 2 : firstItem + j / 2, cf->m_items.size());
				cur_pos_hq = (hq_req && i == firstItem);
				if(!cur_pos_hq && cf->m_items[i].state != STATE_Loading)
					continue;
				if(cur_pos_hq && hq_done)
					continue;
				if(((ret = cf->_loadCoverTex(i, true, cur_pos_hq, false)) == CL_ERROR) && k > 1)
				{
					if(((ret = cf->_loadCoverTex(i, false, cur_pos_hq, false)) == CL_ERROR) && k > 2)
					{
						if((ret = cf->_loadCoverTex(i, true, cur_pos_hq, true)) == CL_ERROR)
							cf->m_items[i].state = STATE_NoCover;
					}
				}
				hq_done = (hq_done == false && ret == CL_OK && cur_pos_hq);
			}
		}
		if(ret == CL_NOMEM && bufferSize > 3)
			bufferSize -= 2;
	}
	cf->m_coverThrdBusy = false;
	return 0;
}
