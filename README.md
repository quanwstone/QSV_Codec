

## 说明

​			QSV：Intel Quick Sync Video,利用Intel图形技术的专用媒体处理能力，快速解码编码，使处理器能够快速完成，提高系统的相应能力。从Sandy Bridge处理器开始，就已经具备了硬件编解码功能. 支持的编解码格式有：HEVC,AVC,MPEG-2，JPEG等.针对QSV技术Intel提供了Intel Media SDK开发工具，通过调用接口可以实现QSV的使用.

## 资料

​			Intel Media SDK：<https://github.com/Intel-Media-SDK/MediaSDK>

​			各显卡支持情况：<https://software.intel.com/zh-cn/articles/intel-graphics-developers-guides>

​			文档：<https://software.intel.com/en-us/documentation/intel-media-server-studios-sdk-referencexxx>



## 流程分析

​			目前主要的接口主要封装在mfxvideo++.h中，主要提供的三个对象为**MFXVideoSession,MFXVideoENCODE，MFXVideoDECODE**。

QSV针对数据的操作利用**Frame Surface Locking**方式，主要原因就是在编解码过程中会涉及到帧数据的拷贝过程，为了减少拷贝过程提高效率从而使用Frame Surface Locking方式.首先申请一个Frame Surface内存池，每个Frame Surface包含一个引用计数,初始为0，当Frame Surface包含数据则计数增加,当计数不为0时则该Frame Surface不能修改，移动，删除或释放,当Frame Surface不在使用时减少计数器,则应用程序可以自由针对Frame Surface操作.通常应用程序不能在使用中增加或减少计数器.



### 解码

1：创建MFXVideoSession,用于启用硬编解码器.
2：创建解码器MFXVideoDECODE.并设置解码器参数.

3：创建存放待解码数据存放内存.

4：解码

### 编码

1：创建MFXVideoSession，用于启用硬件编码器.

2：创建编码器MFXVideoENCODE，并设置编码器参数.

3：创建存放待编码数据内存。

4：编码

### 函数介绍

**MFXInit**：在调用SDK方法前，需要初始化SDK版本库和创建一个SDK Session,这个Session包含一个context用于编码解码中.该方法初始化Session可以指`MFX_IMPL_SOFTWARE`或者`MFX_IMPL_HARDWARE`，前者使用cpu后者使用平台加速功能.也可以指定为`MFX_IMPL_AUTO`或者`MFX_IMPL_AUTO_ANY`，会自动查找最优的SDK版本.

**MFXVideoENCODE_Init**:创建编码器并且通过参数进行初始化.

[**mfxStatus**](https://software.intel.com/node/f233d7ac-0d99-11e6-a837-0800200c9a66) MFXVideoENCODE_Init(mfxSession session, [**mfxVideoParam**](https://software.intel.com/node/f2338981-0d99-11e6-a837-0800200c9a66) *par)



该mfxVideoParam结构体包含配置参数用于编码解码转换视频预处理等.

typedef struct _mfxVideoParam {

​      mfxU32      AllocId;

​      mfxU32      reserved[2];

​      mfxU16      reserved3;

​      mfxU16      AsyncDepth;指定在显示同步结果之前执行多少次异步操作.如果为0则不指定该值.

​      union {

​            [**mfxInfoMFX**](https://software.intel.com/node/f233627b-0d99-11e6-a837-0800200c9a66)        mfx;与编码解码转换有关的配置.

​            [**mfxInfoVPP**](https://software.intel.com/node/f233627c-0d99-11e6-a837-0800200c9a66)        vpp;与预处理有关的配置

​      }

​      mfxU16            Protected;指定内容保护机制，这是一个保留参数，必须为0

​      mfxU16            IOPattern;输入输出内存访问类型，可以使用查询方法获取本地支持的类型，编码必须指定输入模式，解码必须指定输出模式，对于VPP必须指定输入输出模式，参考没聚齐IOPattern

​      [**mfxExtBuffer**](https://software.intel.com/node/f2331452-0d99-11e6-a837-0800200c9a66#mfxExtBuffer)      **ExtParam;

​      mfxU16            NumExtParam;

​      mfxU16            reserved2;

} mfxVideoParam;

此结构指定编码解码转换过程中的配置，任何一个字段为0都表示没有指定该字段.

typedef struct {

​    mfxU32  reserved[7];

 

​    mfxU16  LowPower;为编码器设置此标志，减少功耗和GPU使用.参考CodingOptionValue 枚举

​    mfxU16  BRCParamMultiplier;指定码率控制系数,默认不设置

 

​    mfxFrameInfo    FrameInfo;指定帧参数信息

​    mfxU32  CodecId;指定编码器格式 通过CodecFormatFourCC枚举.

​    mfxU16  CodecProfile;指定CodecProfile，通过CodecProfile枚举

​    mfxU16  CodecLevel;指定CodecLevel，通过CodecLevel枚举

​    mfxU16  NumThread;

 

​    union {

​        struct {   /* MPEG-2/H.264 Encoding Options */

​            mfxU16  TargetUsage;

 

​            mfxU16  GopPicSize;指定GOPsize，如果为0则认为没有指定，如果为1则认为只有I帧，

​            mfxU16  GopRefDist;指定I帧与P帧间隔,如果为0则认为没有指定，如果为1则认为没有B帧.

​            mfxU16  GopOptFlag;GOP规范，通过GopOptFlag枚举设定(是否引用前一个GOP中的I帧)

​            mfxU16  IdrInterval;针对H264如果为0则每个I帧为IDR帧,如果为1则每隔一个I帧为IDR帧.针对HEVC如果为0则只有第一个I帧为IDR帧,如果为1则每个I帧都是IDR，如果为2则每隔一个I帧为IDR帧.

 

​            mfxU16  RateControlMethod;码率控制方式,查看RateControlMethod枚举

​            union {

​                mfxU16  InitialDelayInKB;1 解码器解码缓存达到当前大小才会进行解码,默认设置为0

​                mfxU16  QPI;2

​                mfxU16  Accuracy; 3

​            };

​            mfxU16  BufferSizeInKB;

​            union {

​                mfxU16  TargetKbps;1	目标生成码率必须设置

​                mfxU16  QPP;2

​                mfxU16  ICQQuality;3

​            };

​            union {

​                mfxU16  MaxKbps;1	最大码率

​                mfxU16  QPB;2

​                mfxU16  Convergence;3

​            };

 			1：三个参数用于设置CBR和VBR，

​			 2：三个参数用于设置CQP

​			 3：三个参数用于设置AVBR

​            mfxU16  NumSlice;

​            mfxU16  NumRefFrame;

​            mfxU16  EncodedOrder;

​        };

​        struct {   /* H.264, MPEG-2 and VC-1 Decoding Options */

​            mfxU16  DecodedOrder;

​            mfxU16  ExtendedPicStruct;

​            mfxU16  TimeStampCalc;

​            mfxU16  SliceGroupsPresent;

​            mfxU16  MaxDecFrameBuffering;

​            mfxU16  reserved2[8];

​        };

​        struct {   /* JPEG Decoding Options */

​            mfxU16  JPEGChromaFormat;

​            mfxU16  Rotation;

​            mfxU16  JPEGColorFormat;

​            mfxU16  InterleavedDec;

​            mfxU16  reserved3[9];

​        };

​        struct {   /* JPEG Encoding Options */

​            mfxU16  Interleaved;

​            mfxU16  Quality;

​            mfxU16  RestartInterval;

​            mfxU16  reserved5[10];

​        };

​    };

} mfxInfoMFX;



[**mfxStatus**](https://software.intel.com/node/f233d7ac-0d99-11e6-a837-0800200c9a66) MFXVideoENCODE_Query(mfxSession session, [**mfxVideoParam**](https://software.intel.com/node/f2338981-0d99-11e6-a837-0800200c9a66) *in, [**mfxVideoParam**](https://software.intel.com/node/f2338981-0d99-11e6-a837-0800200c9a66)*out);

用于获取参数初始化值.

当in指针为零，函数返回输出结构的每个字段都有一个非零值.

当in指针不为零，函数将检查输入字段中的有效性，然后返回纠正后的输出值,如果不能纠正则将该字段设置为0.

[**mfxStatus**](https://software.intel.com/node/f233d7ac-0d99-11e6-a837-0800200c9a66) MFXVideoENCODE_QueryIOSurf(mfxSession session, [**mfxVideoParam**](https://software.intel.com/node/f2338981-0d99-11e6-a837-0800200c9a66) *par,[**mfxFrameAllocRequest**](https://software.intel.com/node/f2336276-0d99-11e6-a837-0800200c9a66) *request);

函数返回用于输入frame surface的最小值和建议值.

[**mfxStatus**](https://software.intel.com/node/f233d7ac-0d99-11e6-a837-0800200c9a66) MFXVideoENCODE_EncodeFrameAsync(mfxSession session, [**mfxEncodeCtrl**](https://software.intel.com/node/f2331451-0d99-11e6-a837-0800200c9a66)*ctrl, [**mfxFrameSurface1**](https://software.intel.com/node/f233627a-0d99-11e6-a837-0800200c9a66) *surface, [**mfxBitstream**](https://software.intel.com/node/f2325105-0d99-11e6-a837-0800200c9a66) *bs, mfxSyncPoint *syncp);

函数接收单个输入帧然后生成输出位流

使用mfxEncodeCtrl进行帧类型设置,并不是每一帧都会实时输出，函数返回MFX_ERR_MORE_DATA。如果返回MFX_ERR_NONE则返回值为位流的帧.应用程序的职责是确保输出缓冲区中有足够的空间.最后使用空suface指针调用该方法，重复该调用以耗尽所有剩余的内部缓存位流,直到返回MFX_ERR_MORE_DATA.



[**mfxStatus**](https://software.intel.com/node/f233d7ac-0d99-11e6-a837-0800200c9a66) MFXVideoCORE_SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait);

启动尚未启动的异步函数的执行，并在指定的异步函数完成后返回状态代码，如果wait为0则函数立即返回.

