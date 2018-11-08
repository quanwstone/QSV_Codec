#include<stdio.h>
#include<vector>

extern "C"
{
#include "libavformat/avformat.h"
#include"mfx/mfxastructures.h"
#include"mfx/mfxsession.h"
#include"mfx/mfxvideo.h"
}

#define DEF_HEGITH 240
#define DEF_WIGHT  320
#define QSV_VERSION_MAJOR 1
#define QSV_VERSION_MINOR 1
struct  StructBit
{
	mfxBitstream *pBitstream;
	mfxSyncPoint *pSync;
};
mfxSession		g_mfxSession = NULL;
mfxVideoParam	g_mfxVideoParam;
mfxFrameInfo	g_mfxFrameInfo;
mfxBitstream	*g_mfxBitstream;

std::vector<mfxFrameSurface1 *>g_vecFrameSuface;
std::vector<StructBit *>g_vecBitstream;

mfxStatus AllocMemorySuface()
{
	mfxStatus sts = MFX_ERR_NONE;

	for (int i = 0; i < 4; i++)
	{
		mfxFrameSurface1 *suface = new mfxFrameSurface1;
		memset(suface, 0, sizeof(mfxFrameSurface1));

		suface->Info = g_mfxFrameInfo;
		suface->Data.Pitch = DEF_WIGHT;	//必须赋值，不然在同步取数据会返回MFX_ERR_ABORTED
		suface->Data.PitchLow = DEF_WIGHT;

		suface->Data.Y = new mfxU8[DEF_WIGHT*DEF_HEGITH];
		suface->Data.UV = new mfxU8[DEF_WIGHT * DEF_HEGITH / 2];

		g_vecFrameSuface.push_back(suface);
	}

	for (int i = 0; i < 4; i++)
	{
		StructBit *bit = new StructBit;
		bit->pBitstream = new mfxBitstream;
		
		memset((void*)bit->pBitstream, 0, sizeof(mfxBitstream));

		bit->pBitstream->Data = new mfxU8[DEF_HEGITH *DEF_WIGHT * 4];
		bit->pBitstream->MaxLength = DEF_HEGITH *DEF_WIGHT * 4;
		memset((void*)bit->pBitstream->Data,0, DEF_HEGITH *DEF_WIGHT * 4);

		bit->pSync = new mfxSyncPoint;
		memset((void*)bit->pSync,0,sizeof(mfxSyncPoint));
	
		g_vecBitstream.push_back(bit);
	}
	return sts;
}
mfxFrameSurface1 *GetFreeSuface()
{
	mfxFrameSurface1 *pSuface = NULL;

	for (std::vector<mfxFrameSurface1*>::iterator iter = g_vecFrameSuface.begin();
		iter != g_vecFrameSuface.end(); iter++)
	{
		if (!(*iter)->Data.Locked)
		{
			return *iter;
		}
	}

	return pSuface;
}
StructBit *GetFreebitstream()
{
	StructBit *pbit = NULL;

	for (std::vector<StructBit *>::iterator iter = g_vecBitstream.begin();
		iter != g_vecBitstream.end(); iter++)
	{
		if (*(*iter)->pSync == NULL)
		{
			return *iter;
		}
	}

	return pbit;
}
//NV12
void Encode(unsigned char * pSrc,FILE *pFile)
{
	mfxStatus sts = MFX_ERR_NONE;

	mfxEncodeCtrl enc_ctrl = {0};
	mfxFrameSurface1 *suface1 = NULL;

	suface1 = GetFreeSuface();
	
	memcpy(suface1->Data.Y, pSrc, (DEF_HEGITH*DEF_WIGHT));
	memcpy(suface1->Data.UV, pSrc + DEF_HEGITH*DEF_WIGHT, DEF_HEGITH*DEF_WIGHT / 2);

	StructBit *pBit = GetFreebitstream();

	//if (bIsNextFrameIDR)
	//{
		enc_ctrl.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
	//}
	//else
	//{
		//enc_ctrl.FrameType = MFX_FRAMETYPE_UNKNOWN;
	//}
	
	
	do {
		sts = MFXVideoENCODE_EncodeFrameAsync(g_mfxSession, &enc_ctrl, suface1, pBit->pBitstream, pBit->pSync);

	} while (sts == MFX_WRN_DEVICE_BUSY);
	
	if (sts == MFX_ERR_MORE_DATA)
	{

	}
	if (sts == MFX_ERR_NONE)
	{
		if (*pBit->pSync)
		{
			sts = MFXVideoCORE_SyncOperation(g_mfxSession, *pBit->pSync, 0xFFFFFFFF);
			if (sts == MFX_ERR_NONE)
			{
				fwrite(pBit->pBitstream->Data, 1, pBit->pBitstream->DataLength, pFile);
			}
			pBit->pBitstream->DataLength = 0;
			*pBit->pSync = NULL;
		}	
	}
}
mfxStatus InitVideoCodec()
{
	mfxStatus sts = MFX_ERR_NONE;
	
	/*specifies the codec format identifier in the FOURCC code.
	MFX_CODEC_AVC 
	MFX_CODEC_MPEG2 
	MFX_CODEC_VC1 
	MFX_CODEC_HEVC
	*/
	g_mfxVideoParam.mfx.CodecId = MFX_CODEC_AVC;		
	/*
	number of pictures within the current GOP(Group of Pictures);
	if GopSize=0,then the Gop Size is unspecified.
	if GopSize=1,only I-frames are used.
	*/
	g_mfxVideoParam.mfx.GopPicSize = 0;
	/*
	distance between I or P key frames;if it is zero,the GOP structure is unspecified.
	if GopRefDist=1,there are no B frames used.
	*/
	g_mfxVideoParam.mfx.GopRefDist = 0;
	/*
	additional flags for the GOP specification.
	MFX_GOP_CLOSED
	MFX_GOP_STRICT
	*/
	g_mfxVideoParam.mfx.GopOptFlag = 0; 
	/*
	specifies idrInterval IDR-frame interval in terms of I-frames;
	if idrInterval=0,then every I-frame is an IDR-frame.
	if idrInterval=1,then every other I-frame is an IDR-frame.
	*/
	g_mfxVideoParam.mfx.IdrInterval = 0;
	/*
	number of slices in each video frame.if Numslice equals zero,the encoder may choose any 
	slice partitioning allower by the codec standard
	*/
	g_mfxVideoParam.mfx.NumSlice = 0;
	/*
	Target usage model that guides the encoding process;
	it indicates trade-offs between quality and speed.
	*/
	g_mfxVideoParam.mfx.TargetUsage = 4;//balanced quality and speed
	/*
	Rate control method.
	*/
	g_mfxVideoParam.mfx.RateControlMethod = 1; 
	/*
	TargetKbps must be specified for encoding initialization.
	*/
	g_mfxVideoParam.mfx.TargetKbps = 562;
	/*
	*/
	g_mfxVideoParam.mfx.FrameInfo.FrameRateExtN = 30;
	g_mfxVideoParam.mfx.FrameInfo.FrameRateExtD = 1;

	g_mfxVideoParam.mfx.FrameInfo.CropX = 0;
	g_mfxVideoParam.mfx.FrameInfo.CropY = 0;
	g_mfxVideoParam.mfx.FrameInfo.CropW = DEF_WIGHT;
	g_mfxVideoParam.mfx.FrameInfo.CropH = DEF_HEGITH;
	g_mfxVideoParam.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	g_mfxVideoParam.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	g_mfxVideoParam.mfx.FrameInfo.BitDepthChroma = 8;
	g_mfxVideoParam.mfx.FrameInfo.BitDepthLuma = 8;
	g_mfxVideoParam.mfx.FrameInfo.Shift = 0;
	g_mfxVideoParam.mfx.FrameInfo.Height = DEF_HEGITH;
	g_mfxVideoParam.mfx.FrameInfo.Width = DEF_WIGHT;
	g_mfxVideoParam.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	g_mfxVideoParam.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
	g_mfxVideoParam.AsyncDepth = 4;

	sts = MFXVideoENCODE_Init(g_mfxSession, &g_mfxVideoParam);
	if (sts != MFX_ERR_NONE)
	{

	}
	g_mfxFrameInfo = g_mfxVideoParam.mfx.FrameInfo;

	return sts;
}
mfxStatus InitMFX()
{
	mfxStatus sts = MFX_ERR_NONE;

	mfxIMPL impl = MFX_IMPL_AUTO_ANY;
	mfxVersion ver = { { QSV_VERSION_MINOR, QSV_VERSION_MAJOR } };

	const char *desc;

	sts = MFXInit(impl, &ver, &g_mfxSession);
	if (sts != MFX_ERR_NONE)
		return sts;

	MFXQueryIMPL(g_mfxSession, &impl);

	switch (MFX_IMPL_BASETYPE(impl)) {
	case MFX_IMPL_SOFTWARE:
		desc = "software";
		break;
	case MFX_IMPL_HARDWARE:
	case MFX_IMPL_HARDWARE2:
	case MFX_IMPL_HARDWARE3:
	case MFX_IMPL_HARDWARE4:
		desc = "hardware accelerated";
		break;
	default:
		desc = "unknown";
	}
	/* make sure the decoder is uninitialized */
	MFXVideoENCODE_Close(g_mfxSession);
	return sts;
}
int main(int argc, char *argv[])
{
	av_register_all();

	FILE *pYUV = nullptr;
	FILE *p264 = nullptr;

	memset((void*)&g_mfxVideoParam, 0, sizeof(mfxVideoParam));
	memset((void*)&g_mfxFrameInfo, 0, sizeof(mfxFrameInfo));

	//YUV420p
	errno_t er = fopen_s(&pYUV, "C:\\quanwei\\WMV\\NV12.yuv", "rb+");
	if (er != 0)
	{
		printf("fopen_s Failed\n");
	}
	er = fopen_s(&p264, "C:\\quanwei\\WMV\\264.264", "wb+");
	if (er != 0)
	{
		printf("fopen_s Failed\n");
	}
	//
	mfxStatus sts = InitMFX();
	if (sts != MFX_ERR_NONE)
	{
		printf("InitMFX Failed\n");
	}
	//
	sts = InitVideoCodec();
	if (sts != MFX_ERR_NONE)
	{
		printf("InitVideoCodec Failed\n");
	}
	//
	sts = AllocMemorySuface();
	if (sts != MFX_ERR_NONE)
	{

	}
	//
	unsigned char *pYUV_Buf = new unsigned char[DEF_HEGITH * DEF_WIGHT * 3 / 2];
	
	while (fread(pYUV_Buf, 1, DEF_HEGITH * DEF_WIGHT * 3 / 2, pYUV) != 0)
	{
		Encode(pYUV_Buf,p264);
	}
	
	delete[] pYUV_Buf;

	fclose(p264);
	fclose(pYUV);

	return 0;
}