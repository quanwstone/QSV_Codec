#include<stdio.h>
#include<vector>
#include<windows.h>
/*
利用QSV实现Intel硬件解码。调用的接口是Intel Media SDK提供的接口。
注意:MFXVideoDECODE_DecodeFrameAsync解码一帧Packet并不一定一次完成。需要通过返回值和bitstream的字节长度进行判断。
*/
extern "C"
{
#include "libavformat/avformat.h"
#include"mfx/mfxastructures.h"
#include"mfx/mfxsession.h"
#include"mfx/mfxvideo.h"
}
bool(__cdecl *get_h264_device_type)(int aiLastCodecType, BOOL lbIsEncode);//获取codec类型
void*(__cdecl *encoder_h264_create)(int aiCodecType);//获取编码器
int(__cdecl *encoder_h264_setparam)(void *h, const char *param_name);//设置编码器参数
int(__cdecl *setframeinfo_h264)(void *h, int aiFrameWidth, int aiFrameHeight, int aiBitCount);//设置编码参数
int(__cdecl *encoder_h264_encode)(void *h, char *apSrcData, int aiSrcDataLen, char *apDstBuff, int *aiDstBuffLen, int *abMark);//编码
void(__cdecl *encoder_h264_close)(void * h);//关闭编码器

void* (__cdecl *decoder_h264_create)(int aiCodecType);//获取解码器
int(__cdecl *decoder_h264_decode)(void *h, char *apSrcData, int aiSrcDataLen, char *apDstBuff, int *aiDstBuffLen, int *abMark);//解码
void(__cdecl *decoder_h264_close)(void *h);//关闭解码

HMODULE			m_hCodec5Dll;          //VideoCodec5的DLL句柄	
FILE *pFile = nullptr;
int ig = 0;

BOOL InitVideoCodec5DLL()
{
	if (m_hCodec5Dll != NULL)
		return TRUE;

	m_hCodec5Dll = LoadLibrary(L"VideoCodec5.dll");
	if (m_hCodec5Dll == NULL)
	{
		m_hCodec5Dll = LoadLibrary(TEXT("VideoCodec5.dll"));
		if (m_hCodec5Dll == NULL)
		{
			return FALSE;
		}
	}

	get_h264_device_type =
		reinterpret_cast<bool(__cdecl*)(int, BOOL)>(GetProcAddress(m_hCodec5Dll, "get_h264_device_type"));
	encoder_h264_create =
		reinterpret_cast<void* (__cdecl*)(int)>(GetProcAddress(m_hCodec5Dll, "encoder_h264_create"));
	encoder_h264_setparam =
		reinterpret_cast<int(__cdecl*)(void*, const char *)>(GetProcAddress(m_hCodec5Dll, "encoder_h264_setparam"));
	setframeinfo_h264 =
		reinterpret_cast<int(__cdecl*)(void *, int, int, int)>(GetProcAddress(m_hCodec5Dll, "setframeinfo_h264"));
	encoder_h264_encode =
		reinterpret_cast<int(__cdecl*)(void *, char *, int, char *, int *, int *)>(GetProcAddress(m_hCodec5Dll, "encoder_h264_encode"));
	encoder_h264_close =
		reinterpret_cast<void(__cdecl*)(void*)>(GetProcAddress(m_hCodec5Dll, "encoder_h264_close"));
	decoder_h264_create =
		reinterpret_cast<void* (__cdecl*)(int)>(GetProcAddress(m_hCodec5Dll, "decoder_h264_create"));
	decoder_h264_decode =
		reinterpret_cast<int(__cdecl*)(void *, char *, int, char *, int *, int *)>(GetProcAddress(m_hCodec5Dll, "decoder_h264_decode"));
	decoder_h264_close =
		reinterpret_cast<void(__cdecl*)(void*)>(GetProcAddress(m_hCodec5Dll, "decoder_h264_close"));


	return TRUE;
}
typedef struct MyStruct
{
	mfxFrameSurface1 *psur;
	bool		b;

}MyStruct;
typedef struct MyOutSuface
{
	mfxFrameSurface1 *outsurf;
	mfxSyncPoint *sync;

}MyOutSuface;
typedef std::vector<MyOutSuface *>g_veOutSurface;
typedef std::vector<MyStruct*> g_veFrameSurface;
g_veOutSurface	 m_veOutSurface;

#define QSV_VERSION_MAJOR 1
#define QSV_VERSION_MINOR 1

class CSuface {
public:
	bool InitMxfSession();
	bool AllocSuface(int iW, int iH, int iPitch);
	bool InitCodec();
	int Decode(AVPacket *avpkt,int iOffset);

	MyStruct *GetSuface();
	void	  ReturnSuface();
	g_veFrameSurface m_veFrameSurface;
	//g_veOutSurface	 m_veOutSurface;
	mfxFrameInfo info;
	mfxSession session = nullptr;
};
bool CSuface::AllocSuface(int iW, int iH, int iPitch)
{
	for (int i = 0; i < 10; i++)
	{
		MyStruct *s = new MyStruct;

		mfxFrameSurface1 *psur = (mfxFrameSurface1 *)av_malloc(sizeof(mfxFrameSurface1));
		memset(psur, 0, sizeof(mfxFrameSurface1));

		mfxU8 *Y = (mfxU8 *)new char[iPitch *iH];
		mfxU8 *UV = (mfxU8 *)new char[iPitch *iH];
		memset(Y, 0, iPitch * iH);
		memset(UV, 0, iPitch * iH);

		psur->Data.Y = Y;
		psur->Data.UV = UV;
		psur->Info = info;
		psur->Data.PitchLow = iPitch;
		psur->Data.Pitch = iPitch;

		s->psur = psur;
		s->b = false;

		m_veFrameSurface.push_back(s);
	}

	return true;
}

bool CSuface::InitMxfSession()
{
	mfxIMPL impl = MFX_IMPL_AUTO_ANY;
	mfxVersion ver = { { QSV_VERSION_MINOR, QSV_VERSION_MAJOR } };

	const char *desc;
	int ret;

	ret = MFXInit(impl, &ver, &session);
	if (ret < 0)
		return false;

	MFXQueryIMPL(session, &impl);

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
	MFXVideoDECODE_Close(session);

	return true;
}
bool CSuface::InitCodec()
{
	mfxVideoParam param = { 0 };

	param.mfx.CodecId = MFX_CODEC_AVC;
	param.mfx.FrameInfo.BitDepthLuma = 8;
	param.mfx.FrameInfo.BitDepthChroma = 8;
	param.mfx.FrameInfo.Shift = 0;
	param.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	param.mfx.FrameInfo.Width =1280;
	param.mfx.FrameInfo.Height = 720;
	param.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	param.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	param.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
	param.AsyncDepth = 4;

	int ret = MFXVideoDECODE_Init(session, &param);
	if (ret < 0)
	{
		return false;
	}
	info = param.mfx.FrameInfo;

	AllocSuface(1280,720,1280);

	return true;
}
#define MSDK_ALIGN32(X) (((mfxU32)((X)+31)) & (~ (mfxU32)31))
#define MSDK_DEC_WAIT_INTERVAL 300000

void CSuface::ReturnSuface()
{
	std::vector<MyStruct*>::iterator iter;
	for (iter = m_veFrameSurface.begin(); iter != m_veFrameSurface.end(); iter++)
	{
		if ((*iter)->b && !(*iter)->psur->Data.Locked)
		{
			(*iter)->b = false;
			memset((*iter)->psur->Data.Y,0,(*iter)->psur->Data.Pitch *(*iter)->psur->Info.Height);
			memset((*iter)->psur->Data.UV, 0, (*iter)->psur->Data.Pitch *(*iter)->psur->Info.Height);
		}
	}
}
MyStruct *CSuface::GetSuface()
{
	ReturnSuface();

	std::vector<MyStruct*>::iterator iter;
	for (iter = m_veFrameSurface.begin(); iter != m_veFrameSurface.end(); iter++)
	{
		if (!(*iter)->b && !(*iter)->psur->Data.Locked)
		{
			return *iter;
		}
	}

	MyStruct *s = new MyStruct;

	mfxFrameSurface1 *psur = (mfxFrameSurface1 *)av_malloc(sizeof(mfxFrameSurface1));
	memset(psur, 0, sizeof(mfxFrameSurface1));

	mfxU8 *Y = (mfxU8 *)new char[1280 *720];
	mfxU8 *UV = (mfxU8 *)new char[1280 * 720];
	memset(Y, 0, 1280 * 720);
	memset(UV, 0, 1280 * 720);

	psur->Data.Y = Y;
	psur->Data.UV = UV;
	psur->Info = info;
	psur->Data.PitchLow = 1280;
	psur->Data.Pitch = 1280;

	s->psur = psur;
	s->b = false;

	m_veFrameSurface.push_back(s);

	return s;
}
//解码出的数据为NV12，也就是YUV420，排列方式为Y Y Y Y.... U V U V U V，两个Planer
int CSuface::Decode(AVPacket *avpkt,int iOffset)
{
	mfxFrameSurface1 *insurf = nullptr;
	mfxFrameSurface1 *outsurf = nullptr;
	
	mfxSyncPoint *sync = nullptr;
	MyStruct *st = nullptr;
	int ret;
	int iSize = 0;

	sync = (mfxSyncPoint *)av_malloc(sizeof(*sync));
	if (sync)
		memset(sync, 0, sizeof(*sync));

	do {
		mfxBitstream bs = { { { 0 } } };

		if (avpkt->size) {
			bs.Data = avpkt->data + iOffset;
			bs.DataLength = avpkt->size - iOffset;
			bs.MaxLength = bs.DataLength;
			bs.TimeStamp = avpkt->pts;
		}
		st = GetSuface();
		insurf = st->psur;

		ret = MFXVideoDECODE_DecodeFrameAsync(session, &bs,//需要通过bs的成员判断是否操作完成，存在一帧数据多次调用。而且需要每次都要新的bs局部对象。不然会提示参数被修改
			insurf, &outsurf, sync);

		iSize = bs.DataLength;
		if (ret == MFX_WRN_DEVICE_BUSY)
			Sleep(500);

	} while (ret == MFX_WRN_DEVICE_BUSY || ret == MFX_ERR_MORE_SURFACE);
	if (ret == MFX_WRN_VIDEO_PARAM_CHANGED)
	{
		ret = MFX_ERR_NONE;
	}
	if (*sync)
	{
		MyOutSuface *outs = new MyOutSuface;
		
		mfxFrameSurface1 *outsurface = (mfxFrameSurface1 *)malloc(sizeof(mfxFrameSurface1));
		memcpy(outsurface, outsurf, sizeof(mfxFrameSurface1));

		outs->outsurf = outsurface;
		outs->sync = sync;

		m_veOutSurface.push_back(outs);

		st->b = true;

		return iSize;
	}
	else
	{
		av_freep(&sync);
	}
	if (m_veOutSurface.size() == 4)
	{
		std::vector<MyOutSuface *>::iterator iter = m_veOutSurface.begin();
		if ((*iter)->sync)
		{
			st->b = false;
			ig++;
			do {
				ret = MFXVideoCORE_SyncOperation(session, *(*iter)->sync, MSDK_DEC_WAIT_INTERVAL);
			} while (ret == MFX_WRN_IN_EXECUTION);
			//outsurf->Data.Y
			if (ret == MFX_ERR_NONE)
			{
				outsurf = (*iter)->outsurf;
				for (int i = 0; i < outsurf->Info.CropH; i++)
				{
					fwrite(outsurf->Data.Y + i * outsurf->Data.Pitch, 1, outsurf->Info.CropW, pFile);//y
				}
				for (int i = 0; i < outsurf->Info.CropH / 2; i++)//uv
				{
					for (int j = 0; j < outsurf->Info.CropW; j += 2)
					{
						fwrite(outsurf->Data.UV + i * outsurf->Data.Pitch + j, 1, 1, pFile);
					}
				}
				for (int i = 0; i < outsurf->Info.CropH / 2; i++)//uv
				{
					for (int j = 1; j < outsurf->Info.CropW; j += 2)
					{
						fwrite(outsurf->Data.UV + i * outsurf->Data.Pitch + j, 1, 1, pFile);
					}
				}
			}
			printf("MFXVideoCORE_SysnOperation=%d\n", ig);
		}
		m_veOutSurface.erase(iter);
	}
	if (ret == MFX_ERR_MORE_DATA)
	{
		st->b = true;
	}

	return iSize;
}
int main(int argc, char *argv[])
{
	printf("Begin\n");
	getchar();
	printf("End\n");
	CSuface suface;

	av_register_all();
	
	AVFormatContext *pContext = nullptr;
	AVCodec *pCodec = nullptr;

	errno_t er = fopen_s(&pFile, "C:\\quanwei\\D1.yuv", "wb+");

	int ier = avformat_open_input(&pContext, "C:\\quanwei\\熊出没·奇幻空间.1080P.HD国语中字 00_11_00-00_12_00~1_W1280_H720_F26_Q100_ES.264", NULL, NULL);
	if (ier != 0)
	{
		printf("avformat_open_input Failed\n");
	}
	//Find Stream info
	ier = avformat_find_stream_info(pContext, nullptr);
	if (ier < 0) {
		printf("avformat_find_stream_info faile\n");
	}
	int nVtype = -1;
	ier = av_find_best_stream(pContext, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);
	if (ier < 0)
	{
		printf("av_find_best_stream faile\n");
	}
	nVtype = ier;

	bool br = suface.InitMxfSession();
	if (!br)
	{
		printf("");
	}

	br = suface.InitCodec();
	if (br)
	{
		printf("");
	}

	////初始化videocdec5
	//BOOL bre = InitVideoCodec5DLL();
	//if (!bre)
	//{
	//	printf("InitVideoCodec5DLL Failed\n");
	//}
	//HANDLE	m_pDecode = decoder_h264_create(1);
	//setframeinfo_h264(m_pDecode, 320, 240, 24);//

	AVPacket pk;

	int iMark = 1;
	int iLen = 1280*720*3;
	char *pData = new char[iLen];
	memset(pData, 0, iLen);

	int iSize = 0;
	int iOffset = 0;
	while (true)
	{
		ier = av_read_frame(pContext, &pk);
		if (ier == 0)
		{
			if (pk.stream_index == nVtype) {//Video
				iOffset = 0;
				do 
				{
					iSize = suface.Decode(&pk, iOffset);
					iOffset = pk.size - iSize;
				} while (iSize);
				//iSize = suface.Decode(&pk);

				//memset(pData, 0, iLen);
				//int ire = decoder_h264_decode(m_pDecode, (char*)pk.data, pk.size, pData, &iLen, &iMark);
				//if (ire)
				//{
				//	fwrite(pData, 1, iLen, pFile);
				//	i++;
				//	if (i == 10)
				//	{
				//		fclose(pFile);
				//		return 0;
				//	}
				//	printf("decode success\n");

				//}
			}
		}
		if (ier < 0)
		{
			break;
		}
		av_packet_unref(&pk);

	}
	fclose(pFile);

	return 0;
}