#define  _CRT_SECURE_NO_DEPRECATE 1
#include <windows.h>
#include <stdio.h>
#include <math.h>

#include "ScriptInterpreter.h"
#include "ScriptError.h"
#include "ScriptValue.h"

#include "resource.h"
#include "filter.h"

/* Global definitions */

static FilterDefinition *fd_DMACFilter;
char *dstbufframe;
char *dstbufline;
char *tagline;
char *noise;

/* D-MAC filter definition */

typedef struct MACFilterData {
	bool macMode;
	int ecSeed;
	bool cutMode;
	bool ecOn;
	bool decDelay;
	int decMode;
} MACFilterData;

int RunProcDMACFilter(const FilterActivation *fa, const FilterFunctions *ff);
int StartProcDMACFilter(FilterActivation *fa, const FilterFunctions *ff);
int EndProcDMACFilter(FilterActivation *fa, const FilterFunctions *ff);
int ConfigProcDMACFilter(FilterActivation *fa, const FilterFunctions *ff, HWND hwnd);
void StringProcDMACFilter(const FilterActivation *fa, const FilterFunctions *ff, char *str);
int InitProcDMACFilter(FilterActivation *fa, const FilterFunctions *ff);
long ParamProcDMACFilter(FilterActivation *fa, const FilterFunctions *ff);
void RGB2YUV(int r,int g,int b,int &Y,int &U,int &V);
void YUV2RGB(int Y,int U,int V,int &r,int &g,int &b);
int strToBin(const char * str);

bool FssProcDMACFilter(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	MACFilterData *mfd = (MACFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d, %d, %d, %d, %d)",
		mfd->macMode,
		mfd->ecSeed,
		mfd->cutMode,
		mfd->ecOn,
		mfd->decDelay);
	return true;
}

void ScriptConfigDMACFilter(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;
	MACFilterData *mfd = (MACFilterData *)fa->filter_data;

	mfd->macMode	= !!argv[0].asInt();
	mfd->ecSeed		= argv[1].asInt();
	mfd->cutMode	= !!argv[2].asInt();
	mfd->ecOn		= !!argv[3].asInt();
	mfd->decDelay	= !!argv[4].asInt();
}

ScriptFunctionDef func_defsdmac[]={
	{ (ScriptFunctionPtr)ScriptConfigDMACFilter, "Config", "0iiiii" },
	{ NULL },
};

CScriptObject script_objmac={
	NULL, func_defsdmac
};

FilterDefinition filterDef_DMACFilter=
{
	NULL, NULL, NULL,			// next, prev, module
	"DMAC: MAC/Eurocrypt",		// name
	"Converts between RGB and MAC/YUV separated formats\n\n" \
	"Optional Eurocrypt encoder using either single or double cut system",		// desc
	"http://filmnet.plus",		// maker
	NULL,						// private_data
	sizeof(MACFilterData),		// inst_data_size
	InitProcDMACFilter,         // initProc
	NULL,                       // deinitProc
	RunProcDMACFilter,			// runProc
	ParamProcDMACFilter,		// paramProc
	ConfigProcDMACFilter,		// configProc
	StringProcDMACFilter,		// stringProc
	StartProcDMACFilter,		// startProc
	EndProcDMACFilter,			// endProc
	&script_objmac,				// script_obj
	FssProcDMACFilter,			// fssProc
};


int RunProcDMACFilter(const FilterActivation *fa, const FilterFunctions *ff)
{
	/* Pointer to MFD */
	MACFilterData *mfd = (MACFilterData *)fa->filter_data;

	/* Pixel pointers to source and destination frame stores */
	Pixel32 *src= fa->src.data;
	Pixel32 *dst= fa->dst.data;

	/* Char pointers to source and destination frame stores */
	char *srce = (char *)fa->src.data;
	char *dste = (char *)fa->dst.data;

	/* Get line length */
	int dstline = fa->dst.w*sizeof(Pixel32)+fa->dst.modulo;
	int srcline = fa->src.w*sizeof(Pixel32)+fa->src.modulo;

	/* Source and target width/address */
	int srcx = fa->src.w*sizeof(Pixel32);
	int destx = fa->dst.w*sizeof(Pixel32);

	int frame = fa->pfsi->lCurrentSourceFrame; // frame number
	int shift;

	/* Convert from RGB to MAC */
	/* Init source width and height */
	int desty = fa->dst.h; // Destination frame height
	const int w = fa->src.w;
	const int h = fa->src.h;



	/* Convert to MAC/Eurocrypt */
	if(mfd->macMode) 
	{

		/* Difference in width between source and target */
		const int dy = fa->dst.w - w;

		const int dataw = w/4;

		const int  _noise[2] = {0xFF,0x00};
		  
		/* Extract full frame Y component */

		for (int y=0; y<h; y++)
		{

			for (int x=0; x<w; x++)
			{
				int Y,U,V;
			  	int r= (src[x]>>16)&0xff;
			  	int g= (src[x]>> 8)&0xff;
			  	int b= (src[x]    )&0xff;

				/* Convert to YUV */
				RGB2YUV (r,g,b,Y,U,V);

				dst[x+dy ]= (Y<<24)|(Y<<16)|(Y<<8)|Y; // Y

				if ( x < dataw) {
					int n = _noise[rand()%2];	
					dst[x] = (n<<24)|(n<<16)|(n<<8)|n; 
				}

			}
			
			/* Move onto next line */
			dst+= fa->dst.pitch>>2;
			src+= fa->src.pitch>>2;
		}

		/* Go back to start of frame (bottom left) */
		dst-=((fa->dst.pitch>>2)*h);
		src-=((fa->src.pitch>>2)*h);

		/* Extract full frame from half frame UV component */

		for (int y=0; y<(int)h/2; y++)
		{
			for (int x=0; x<w; x++)
			{
				int Y,U,V;
				int r= (src[x]>>16)&0xff;
				int g= (src[x]>> 8)&0xff;
				int b= (src[x]    )&0xff;

				/* Convert to YUV */
				RGB2YUV (r,g,b,Y,U,V);

				dst[(int)dataw+(x/2)+(fa->dst.pitch>>2)]= (U<<24)|(U<<16)|(U<<8)|U;	// U
				dst[(int)dataw+(x/2)]= (V<<24)|(V<<16)|(V<<8)|V;						// V
			}
	
			/* Move onto next line */
			dst+= fa->dst.pitch>>1;
			src+= fa->src.pitch>>1;
		}

		if(mfd->ecOn == TRUE) {

			/* 
			Tag format 
			==========
			First 7 high bits:	1001001 = Eurocrypt
								1001000 = Trigger
			Rest of bits: randomised
			*/

			char seed[1024]="";
			int  _tags[2] = {0xFF,0x00};
			int tag[32];


			/* Check if you're encrypting an already encrypted video */
			// Grab tag line
			srce +=srcline*(h-1);
			memcpy(tagline,srce,srcx);
			srce -=srcline*(h-1);

			// Read tag for Eurocrypt tag - first 7 bits 
			tag[0] = (tagline[0] >= 0 ? 0 : 1);
			sprintf(seed, "%i", tag[0]);
			for (int i=1; i < 7;i++) {
				tag[i] = (tagline[((srcx/256)*4)*i] >= 0 ? 0 : 1);
				sprintf(seed, "%s%i", seed,tag[i]);
			}

			/* Check for magic number */
			if ( strToBin(seed) != 73 && strToBin(seed) != 72 )
			{
				int ectag[7]={0xFF,0x00,0x00,0xFF,0x00,0x00};

				if ( frame%37 == 0 ) 
				{
					ectag[6] = 0x00;
					sprintf(seed, "%s", "1001000");	
				}
				else
				{
					ectag[6] = 0xFF;
					sprintf(seed, "%s", "1001001");	
				}

				/* Generate seed for TAG LINE based on current frame and mfd->vcSeed value */
				srand((mfd->ecSeed+10)*((frame%256)+10)); if ((frame+2)%2 == 0) {	srand((rand()) + rand()); }	else { srand(rand() * (rand())); }

				for (int i=0; i < 7; i++)
					tag[i] = ectag[i];

				for (int i=7;i<32;i++)
				{
					// Random tag - black or white
					tag[i] = _tags[rand()%2];
					sprintf(seed, "%s%i", seed,(tag[i] == 0xFF ? 1 : 0));
				}

				memset(tagline,0x00,srcx);
				for (int i=0; i < 32; i++) 
				{
					memset(tagline+((srcx/256)*4)*i,tag[i],(srcx/256)*4);
				}
			}
			else 
			{
				/* Read tag for Eurocrypt tag - remaining 25 bits */
				for (int i=7; i < 32;i++) {
					tag[i] = (tagline[((srcx/256)*4)*i] >= 0 ? 0 : 1);
					sprintf(seed, "%s%i", seed,tag[i]);
				}
			}

			srand(strToBin(seed)*mfd->ecSeed);

			if (mfd->cutMode == TRUE) {
				
				/* Encoder single line cut */
				
				/* Cut point locations */
				int min = (int) (destx*0.262);
				int max = (int) (destx*0.217);

				const int dataw4 = dataw*4;

		 		for(int i=0; i<desty; i++)
				{
					shift=(int) 4* (rand() %(max-min)+min + 1);
					shift = shift%destx;
					
					memcpy(dstbufline,dste,dataw4);
					memcpy(dstbufline+dataw4,dste+shift+dataw4,destx-shift-dataw4); // rotate left
					memcpy(dstbufline+(destx-shift),dste+dataw4,shift); //rotate right
					memcpy(dste,dstbufline,destx);
					dste += dstline;
					srce += srcline;
				}  
			} else
			{
				/* Encoder double line cut */

				/* Cut point locations */
				int min = (int) (destx*0.265);
				int max = (int) (destx*0.37);
				const int dataw4 = dataw*4;

				// Luma width (needed for double cut)
				int lumaw = destx/7*4;

				// Chroma width (needed for double cut)
				int chromaw = destx-lumaw;
				int chromaw4 = (destx-lumaw)-dataw4;

		 		for(int i=0; i<desty; i++)
				{
					int rshift=(int) 4* (rand() %(max-min)+min + 1);

					memcpy(dstbufline,dste,dataw4);

					// Cut/rotate chroma
					shift = (rshift%chromaw4);
					memcpy(dstbufline+dataw4,dste+shift+dataw4,chromaw4-shift);
					memcpy(dstbufline+(chromaw4-shift)+dataw4,dste+dataw4,shift);
					memcpy(dste,dstbufline,chromaw);

					// Cut/rotate luma
					rshift=(int) 4* (rand() %(max-min)+min + 1);
					shift = (rshift%lumaw);
					memcpy(dstbufline,dste+chromaw+shift,lumaw-shift);
					memcpy(dstbufline+lumaw-shift,dste+chromaw,shift);
					memcpy(dste+chromaw,dstbufline,lumaw);
					dste += dstline;
					srce += srcline;
				}    
			}
			dste -= dstline;
			memset(dste,0xFF,srcx);
			memcpy(dste,tagline,srcx);
		}
	}
	else
	{		

		/* Decrypt here */
		int tag[32];
		char seed[1024]="";
	
		/* Init source and target frame dimensions */

		const int sw= fa->src.w;
		const int dw= fa->dst.w;
		const int h= fa->dst.h;
		const int sy = sw - dw; // difference in width
		const int dataw = dw/4; // data width

		/* Grab a copy of tagline */
		srce +=srcline*(h-1);
		memcpy(tagline,srce,destx);
		srce -=srcline*(h-1);

		tag[0] = (tagline[10] >= 0 ? 0 : 1);
		sprintf(seed, "%i", tag[0]);

		/* Read tag for Videocrypt tag - first 7 bits */
		for (int i=1; i < 7;i++) {
			tag[i] = (tagline[(((destx/256)*4)*i)+10] >= 0 ? 0 : 1);
			sprintf(seed, "%s%i", seed,tag[i]);
		}

		if ( strToBin(seed) == 72 && mfd->decMode > 0) mfd->decMode--;

		/* Check whether frame is tagged for Eurocrypt (magic number 73) and check-box is on */
		if(mfd->ecOn == TRUE && mfd->decMode < 2 && (strToBin(seed) == 73 || strToBin(seed) == 72)) {

			/* Read tag for Eurocrypt tag - remaining 25 bits */
			for (int i=7; i < 32;i++) {
				tag[i] = (tagline[((destx/256)*4)*i] >= 0 ? 0 : 1);
				sprintf(seed, "%s%i", seed,tag[i]);
			}

			if ( mfd->decMode == 1 )
			{
				srand(strToBin(seed)*rand());
			}
			else
			{
				srand(strToBin(seed)*mfd->ecSeed);
			}

			const int dataw4 = dataw * 4;

			if (mfd->cutMode == TRUE) {

				/* Cut point locations for single cut mode */
				int min = (int) (srcx*0.262);
				int max = (int) (srcx*0.217);
				

				for(int i=0; i<desty; i++)
				{
					shift=(int) 4* (rand() %(max-min)+min + 1);
					shift = shift%srcx;
					memcpy(dstbufline+dataw4,srce+(srcx-shift),shift);
					memcpy(dstbufline+shift+dataw4,srce+dataw4,srcx-shift-dataw4);
					memcpy(dstbufframe,dstbufline,srcx);
					//memcpy(srce,dstbufframe,srcx);
					srce	+= srcline;
					dstbufframe	+= srcline;
				}

			} else {

				// Luma width (needed for double cut)
				int lumaw = srcx/7*4;

				// Chroma width (needed for double cut)
				int chromaw = srcx-lumaw;
				int chromaw4 = (srcx-lumaw)-dataw4;

				/* Cut point locations for double cut mode */
				int min = (int) (srcx*0.265);
				int max = (int) (srcx*0.37);

		 		for(int i=0; i<desty; i++)
				{

					int rshift=(int) 4* (rand() %(max-min)+min + 1);
					// Uncut/rotate chroma 
					shift = (rshift%chromaw4);
					memcpy(dstbufline+dataw4,srce+dataw4+chromaw4-shift,shift);
					memcpy(dstbufline+shift+dataw4,srce+dataw4,chromaw4-shift);
					memcpy(dstbufframe,dstbufline,chromaw);

					// Uncut/rotate luma
					rshift=(int) 4* (rand() %(max-min)+min + 1);
					shift = (rshift%lumaw);
					memcpy(dstbufline,srce+srcx-shift,shift);
					memcpy(dstbufline+shift,srce+chromaw,lumaw-shift);
					memcpy(dstbufframe+chromaw,dstbufline,lumaw);

					srce	+= srcline;
					dstbufframe	+= srcline;
				}  
			}
				
		} else 
		{
			for(int i=0; i<desty; i++)
			{
				memcpy(dstbufframe,srce,srcx);

				srce	+= srcline;
				dstbufframe	+= srcline;
				if ( mfd->decDelay ) mfd->decMode = 2;
			}
		}

		dstbufframe	-= srcline*desty;

		/* Convert from MAC to RGB */




		Pixel32 *srcbuf = (Pixel32*) dstbufframe;

		/* Combine full frame Y component */

		for (int y=0; y<h-1; y++)
		{
			for (int x=0; x<sw; x++)
			{
				dst[x]= (srcbuf[x+sy]&0xff00); // Y
			}

			/* Next line */
			dst+= fa->dst.pitch>>2;
			srcbuf+= fa->src.pitch>>2;
		 }

		/* Rewind */
		dst-=((fa->dst.pitch>>2)*(h-1));
		srcbuf-=((fa->src.pitch>>2)*(h-1));

		/* Extract full frame UV component */

		for (int y=0; y<(h-1)/2; y++)
		{
			for (int x=0; x<dw; x++)
			{	
				dst[x] = (srcbuf[(int)dataw+(x/2)+(fa->src.pitch>>2)]&0xff0000)|(dst[x]);	// U
				dst[x] = (srcbuf[(int)dataw+(x/2)]&0xff)|(dst[x]);						// V
			}
		  
			/* Next line */
			srcbuf+= fa->src.pitch>>2;
			dst+= fa->dst.pitch>>2;

			for (int x=0; x<dw; x++)
			{
				dst[x] = (srcbuf[(int)dataw+(x/2)+(fa->src.pitch>>2)]&0xff)|(dst[x]);	// V
				dst[x] = (srcbuf[(int)dataw+(x/2)]&0xff0000)|(dst[x]);				// U
				
			}

			/* Next line */
			dst+= fa->dst.pitch>>2;
			srcbuf+= fa->src.pitch>>2;
		 } 

		char buff[100];
		sprintf(buff,"sw: %i, dw: %i, dataw: %i", (int)sw, (int)dw, (int)dataw);
		//MessageBox(0,buff,"Info",0);

		/* Rewind back */
		dst-=((fa->dst.pitch>>2)*(h-2));
		srcbuf-=((fa->src.pitch>>2)*(h-2));

		/* Convert combined frame */
		 for (int y=0; y<h-1; y++)
		 {
			for (int x=0; x<dw; x++)
			{
				int r,g,b;

				int Y= (*dst>> 8) & 0xff;
				int U= (*dst>>16) & 0xff;
				int V= (*dst    ) & 0xff;
				YUV2RGB (Y,U,V,r,g,b);
				*dst++= (r<<16) | (g<<8) | b;
			 }
		 } 
		memcpy(dst,tagline,destx);
		dst-=((fa->dst.pitch>>2));
		memset(dst,0x00,destx);
		
	}
	return 0;
}

int InitProcDMACFilter(FilterActivation *fa, const FilterFunctions *ff) {
	
	/* Pointer to MFD */
	MACFilterData *mfd = (MACFilterData *)fa->filter_data;

	/* Default mode */
	mfd->macMode = TRUE;
	mfd->ecSeed = 9876;
	mfd->cutMode = TRUE;
	mfd->ecOn = TRUE;
	mfd->decDelay = TRUE;

	return 0;
}

int StartProcDMACFilter(FilterActivation *fa, const FilterFunctions *ff)
{
	MACFilterData *mfd = (MACFilterData *)fa->filter_data;

	/* Allocate frame and line buffers */
	if(mfd->macMode) {
		dstbufline = (char*)malloc(fa->dst.w*sizeof(Pixel32));
		tagline = (char*)malloc(fa->src.w*sizeof(Pixel32));
	} else
	{
		dstbufline  = (char*)malloc(fa->src.w*sizeof(Pixel32));
		dstbufframe = (char*)malloc((fa->src.w*sizeof(Pixel32)+fa->src.modulo)*(fa->src.h));
		tagline = (char*)malloc(fa->dst.w*sizeof(Pixel32));
	}

	mfd->decMode = mfd->decDelay == TRUE ? 2 : 0;

    return 0;
}

int EndProcDMACFilter(FilterActivation *fa, const FilterFunctions *ff)
{
    return 0;
}

/* Routine to update text in filter window */
void StringProcDMACFilter(const FilterActivation *fa, const FilterFunctions *ff, char *str) {
 
	/* Pointer to MFD */
	MACFilterData *mfd = (MACFilterData *)fa->filter_data;

	/* Array of possible configurations */
	const char *modes[2]={
		"from",
		"to"
    };

	const char *cutModes[2]={
		"double",
		"single"
	};

	const char *ecOns[2]={
		"off",
		"on"
	};
 
	sprintf(str, " (%s MAC, Eurocrypt %s, %s cut, %i, decode delay %s)", modes[mfd->macMode],ecOns[mfd->ecOn],cutModes[mfd->cutMode],mfd->ecSeed,ecOns[mfd->decDelay]);
}



long ParamProcDMACFilter(FilterActivation *fa, const FilterFunctions *ff)
{

	/* Pointer to MFD */
	MACFilterData *mfd = (MACFilterData *)fa->filter_data;

	/* Define target width depending on mode */
	if(mfd->macMode) 
	{
		fa->dst.w = (int) 7* (fa->src.w/4);
	}
	else
	{
		fa->dst.w = (int) 4* (fa->src.w/7);
	}

	fa->dst.h = fa->src.h;
	fa->dst.AlignTo8();

	return FILTERPARAM_SWAP_BUFFERS;
}

BOOL CALLBACK macConfigDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    MACFilterData *mfd = (MACFilterData *)GetWindowLong(hdlg, DWL_USER);

	/* Temporary storage for strings */
	char tmp[100];

    switch(msg) {
        case WM_INITDIALOG:
            SetWindowLong(hdlg, DWL_USER, lParam);
            mfd = (MACFilterData *)lParam;
			
            CheckDlgButton(hdlg, IDR_TOMAC, mfd->macMode?BST_CHECKED:BST_UNCHECKED);
            CheckDlgButton(hdlg, IDR_FROMMAC, mfd->macMode?BST_UNCHECKED:BST_CHECKED);
			CheckDlgButton(hdlg, IDC_SINGLECUT, mfd->cutMode?BST_CHECKED:BST_UNCHECKED);
            CheckDlgButton(hdlg, IDC_DOUBLECUT, mfd->cutMode?BST_UNCHECKED:BST_CHECKED);
			CheckDlgButton(hdlg, IDC_ECON, mfd->ecOn?BST_CHECKED:BST_UNCHECKED);
			CheckDlgButton(hdlg, IDC_DECDELAY, mfd->decDelay?BST_CHECKED:BST_UNCHECKED);
			sprintf_s(tmp,"%i",mfd->ecSeed);
			SetDlgItemText(hdlg, IDC_ECSEED, tmp );

            return TRUE;

        case WM_COMMAND:
            switch(LOWORD(wParam)) {
            case IDC_OKM:
				mfd->macMode = !!IsDlgButtonChecked(hdlg, IDR_TOMAC);
				mfd->cutMode = !!IsDlgButtonChecked(hdlg, IDC_SINGLECUT);
				mfd->ecOn = !!IsDlgButtonChecked(hdlg, IDC_ECON);
				mfd->decDelay = !!IsDlgButtonChecked(hdlg, IDC_DECDELAY);
				GetDlgItemText(hdlg, IDC_ECSEED, tmp,10);
				mfd->ecSeed = atoi(tmp);
                EndDialog(hdlg, 0);
                return TRUE;
            case IDC_CANCELM:
                EndDialog(hdlg, 1);
                return FALSE;
            }
            break;
    }

    return FALSE;
}

int ConfigProcDMACFilter(FilterActivation *fa, const FilterFunctions *ff, HWND hwnd)
{
	/* Open config window when loading filter */
	return DialogBoxParam(fa->filter->module->hInstModule, MAKEINTRESOURCE(IDD_MACCONFIG), hwnd, macConfigDlgProc, (LPARAM)fa->filter_data);
}




/* General routines to register filters */

extern "C" int __declspec(dllexport) __cdecl VirtualdubFilterModuleInit2(FilterModule *fm, const FilterFunctions *ff, int& vdfd_ver, int& vdfd_compat);
extern "C" void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(FilterModule *fm, const FilterFunctions *ff);

int __declspec(dllexport) __cdecl VirtualdubFilterModuleInit2(FilterModule *fm, const FilterFunctions *ff, int& vdfd_ver, int& vdfd_compat)
{
	if (!(fd_DMACFilter = ff->addFilter(fm, &filterDef_DMACFilter, sizeof(FilterDefinition))))
        return 1;

	vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
	vdfd_compat = VIRTUALDUB_FILTERDEF_COMPATIBLE;

    return 0;
}

void __declspec(dllexport) __cdecl VirtualdubFilterModuleDeinit(FilterModule *fm, const FilterFunctions *ff)
{
	ff->removeFilter(fd_DMACFilter);
}




/* Conversion table courtesy of Emiliano Ferrari */
void RGB2YUV(int r,int g,int b,int &Y,int &U,int &V)
{
	// input:  r,g,b [0..255]
	// output: Y,U,V [0..255]
	const int Yr= (int)( 0.257*65536.0*255.0/219.0);
	const int Yg= (int)( 0.504*65536.0*255.0/219.0);
	const int Yb= (int)( 0.098*65536.0*255.0/219.0);
	const int Ur= (int)( 0.439*65536.0*255.0/223.0);
	const int Ug= (int)(-0.368*65536.0*255.0/223.0);
	const int Ub= (int)(-0.071*65536.0*255.0/223.0);	
	const int Vr= (int)(-0.148*65536.0*255.0/223.0);
	const int Vg= (int)(-0.291*65536.0*255.0/223.0);
	const int Vb= (int)( 0.439*65536.0*255.0/223.0);

	Y= ((int)(Yr * r + Yg * g + Yb * b+32767)>>16);
	U= ((int)(Ur * r + Ug * g + Ub * b+32767)>>16)+128;
	V= ((int)(Vr * r + Vg * g + Vb * b+32767)>>16)+128;
	if ((unsigned)U>255) U=(~U>>31)&0xff;
	if ((unsigned)V>255) V=(~V>>31)&0xff;
}

/* Conversion table courtesy of Emiliano Ferrari */
void YUV2RGB(int Y,int U,int V,int &r,int &g,int &b)
{
	// input:  Y,U,V [0..255]
	// output: r,g,b [0..255]
	const int YK= (int)(1.164*65536.0*219.0/255.0);
	const int k1= (int)(1.596*65536.0*223.0/255.0);
	const int k2= (int)(0.813*65536.0*223.0/255.0);
	const int k3= (int)(2.018*65536.0*223.0/255.0);
	const int k4= (int)(0.391*65536.0*223.0/255.0);

	Y*= YK;
	U-= 128;
	V-= 128;

	r= (Y+k1*U+32768)>>16;
	g= (Y-k2*U-k4*V+32768)>>16;
	b= (Y+k3*V+32768)>>16;

	if ((unsigned)r>255) r=(~r>>31)&0xff;
	if ((unsigned)g>255) g=(~g>>31)&0xff;
	if ((unsigned)b>255) b=(~b>>31)&0xff;
}

int strToBin(const char * str)
{
    int val = 0;

    while (*str != '\0')
        val = 2 * val + (*str++ - '0');
    return val;
}
