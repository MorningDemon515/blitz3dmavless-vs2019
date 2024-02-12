
#include "std.h"
#include "gxgraphics.h"
#include "gxruntime.h"

extern gxRuntime *gx_runtime;

gxGraphics::gxGraphics( gxRuntime *rt,IDirectDraw *dd,IDirectDrawSurface *fs,IDirectDrawSurface *bs,bool d3d ):
runtime(rt),dirDraw(dd),dir3d(0),dir3dDev(0),def_font(0),gfx_lost(false),dummy_mesh(0){

	dirDraw->QueryInterface( IID_IDirectDraw,(void**)&ds_dirDraw );

	front_canvas=d_new gxCanvas( this,fs,0 );
	back_canvas=d_new gxCanvas( this,bs,0 );

	front_canvas->cls();
	back_canvas->cls();

	FT_Init_FreeType(&ftLibrary);

	def_font = nullptr;
	//def_font=loadFont( "courier",12,0 );

	front_canvas->setFont( def_font );
	back_canvas->setFont( def_font );

	memset(&primFmt,0,sizeof(primFmt));
	primFmt.dwSize=sizeof(primFmt);
	fs->GetPixelFormat( &primFmt );

	//are we fullscreen?
	_gamma=0;
	if( fs!=bs ){
		if( fs->QueryInterface( IID_IDirectDrawGammaControl,(void**)&_gamma )>=0 ){
			if( _gamma->GetGammaRamp( 0,&_gammaRamp )<0 ) _gamma=0;
		}
	}
	if( !_gamma ){
		for( int k=0;k<256;++k ) _gammaRamp.red[k]=_gammaRamp.blue[k]=_gammaRamp.green[k]=k;
	}
}

gxGraphics::~gxGraphics(){
	if( _gamma ) _gamma->Release();
#ifdef PRO
	while( scene_set.size() ) freeScene( *scene_set.begin() );
#endif
	while( movie_set.size() ) closeMovie( *movie_set.begin() );
	while( font_set.size() ) freeFont( *font_set.begin() );
	while( canvas_set.size() ) freeCanvas( *canvas_set.begin() );

	set<string>::iterator it;
	for( it=font_res.begin();it!=font_res.end();++it ) RemoveFontResource( (*it).c_str() );
	font_res.clear();

	delete back_canvas;
	delete front_canvas;
	
	FT_Done_FreeType(ftLibrary);

	ds_dirDraw->Release();

	dirDraw->RestoreDisplayMode();
	dirDraw->Release();
}

void gxGraphics::setGamma( int r,int g,int b,float dr,float dg,float db ){
	_gammaRamp.red[r&255]=dr*257.0f;
	_gammaRamp.green[g&255]=dg*257.0f;
	_gammaRamp.blue[b&255]=db*257.0f;
}

void gxGraphics::updateGamma( bool calibrate ){
	if( !_gamma ) return;
	_gamma->SetGammaRamp( calibrate ? DDSGR_CALIBRATE : 0,&_gammaRamp );
}

void gxGraphics::getGamma( int r,int g,int b,float *dr,float *dg,float *db ){
	*dr=_gammaRamp.red[r&255]/257.0f;
	*dg=_gammaRamp.green[g&255]/257.0f;
	*db=_gammaRamp.blue[b&255]/257.0f;
}

void gxGraphics::backup(){
}

bool gxGraphics::restore() {
	while (dir3dDev->TestCooperativeLevel() != D3D_OK) {
		//if (dir3dDev->TestCooperativeLevel() == D3DERR_WRONGMODE) return false;
		Sleep(100);
	}

	if (back_canvas->getSurface()->IsLost() == D3D_OK) return true;

	// Restore all canvases
	std::set<gxCanvas*>::iterator it;
	for (it = canvas_set.begin(); it != canvas_set.end(); ++it) {
		(*it)->restore();
	}

#ifdef PRO
	// Restore all meshes (b3d surfaces)
	std::set<gxMesh*>::iterator mesh_it;
	for (mesh_it = mesh_set.begin(); mesh_it != mesh_set.end(); ++mesh_it) {
		(*mesh_it)->restore();
	}
	if (device1) device1->EvictManagedResources();//EvictManagedTextures(D3DRENDERSTATE_EVICTMANAGEDTEXTURES
#endif

	return true;
}

gxCanvas *gxGraphics::getFrontCanvas()const{
	return front_canvas;
}

gxCanvas *gxGraphics::getBackCanvas()const{
	return back_canvas;
}

gxFont *gxGraphics::getDefaultFont()const{
	return def_font;
}

void gxGraphics::vwait(){
	dirDraw->WaitForVerticalBlank( DDWAITVB_BLOCKBEGIN,0 );
}

void gxGraphics::flip( bool v ){
	runtime->flip( v );
}

void gxGraphics::copy( gxCanvas *dest,int dx,int dy,int dw,int dh,gxCanvas *src,int sx,int sy,int sw,int sh ){
	RECT r={ dx,dy,dx+dw,dy+dh };
	ddUtil::copy( dest->getSurface(),dx,dy,dw,dh,src->getSurface(),sx,sy,sw,sh );
	dest->damage( r );
}

int gxGraphics::getScanLine()const{
	DWORD t=0;
	dirDraw->GetScanLine( &t );
	return t;
}

int gxGraphics::getTotalVidmem()const{
	DDCAPS caps={sizeof(caps)};
	dirDraw->GetCaps( &caps,0 );
	return caps.dwVidMemTotal;
}

int gxGraphics::getAvailVidmem()const{
	DDCAPS caps={sizeof(caps)};
	dirDraw->GetCaps( &caps,0 );
	return caps.dwVidMemFree;
}

gxMovie *gxGraphics::openMovie( const string &file,int flags ){

	IAMMultiMediaStream *iam_stream;

	if( CoCreateInstance(
		CLSID_AMMultiMediaStream,NULL,CLSCTX_INPROC_SERVER,
        IID_IAMMultiMediaStream,(void **)&iam_stream )==S_OK ){

		if( iam_stream->Initialize( STREAMTYPE_READ,AMMSF_NOGRAPHTHREAD,NULL )==S_OK ){

			if( iam_stream->AddMediaStream( ds_dirDraw,&MSPID_PrimaryVideo,0,NULL )==S_OK ){

				iam_stream->AddMediaStream( NULL,&MSPID_PrimaryAudio,AMMSF_ADDDEFAULTRENDERER,NULL );

				WCHAR *path=new WCHAR[ file.size()+1 ];
				MultiByteToWideChar( CP_ACP,0,file.c_str(),-1,path,sizeof(WCHAR)*(file.size()+1) );
				int n=iam_stream->OpenFile( path,0 );
				delete path;

				if( n==S_OK ){
					gxMovie *movie=d_new gxMovie( this,iam_stream );
					movie_set.insert( movie );
					return movie;
				}
			}
		}
		iam_stream->Release();
	}
	return 0;
}

gxMovie *gxGraphics::verifyMovie( gxMovie *m ){
	return movie_set.count( m ) ? m : 0;
}

void gxGraphics::closeMovie( gxMovie *m ){
	if( movie_set.erase( m ) ) delete m;
}

gxCanvas *gxGraphics::createCanvas( int w,int h,int flags ){
	ddSurf *s=ddUtil::createSurface( w,h,flags,this );
	if( !s ) return 0;
	gxCanvas *c=d_new gxCanvas( this,s,flags );
	canvas_set.insert( c );
	c->cls();
	return c;
}

gxCanvas *gxGraphics::loadCanvas( const string &f,int flags ){
	ddSurf *s=ddUtil::loadSurface( f,flags,this );
	if( !s ) return 0;
	gxCanvas *c=d_new gxCanvas( this,s,flags );
	canvas_set.insert( c );
	return c;
}

gxCanvas *gxGraphics::verifyCanvas( gxCanvas *c ){
	return canvas_set.count( c ) || c==front_canvas || c==back_canvas ? c : 0;
}

void gxGraphics::freeCanvas( gxCanvas *c ){
	if( canvas_set.erase( c ) ) delete c;
}

int gxGraphics::getWidth()const{
	return front_canvas->getWidth();
}

int gxGraphics::getHeight()const{
	return front_canvas->getHeight();
}

int gxGraphics::getDepth()const{
	return front_canvas->getDepth();
}

gxFont *gxGraphics::loadFont( const string &f,int height,int flags ){

	string t;
	int n=f.find('.');
	if( n!=string::npos ){
		t=fullfilename(f);
		if( !font_res.count(t) && AddFontResource( t.c_str() ) ) font_res.insert( t );
		t=filenamefile( f.substr(0,n) );
	}else{
		t=f;
	}

	gxFont* newFont = new gxFont(ftLibrary, this, f, height, flags);
	font_set.emplace(newFont);
	return newFont;
}

gxFont *gxGraphics::verifyFont( gxFont *f ){
	return font_set.count( f ) ? f : 0;
}

void gxGraphics::freeFont( gxFont *f ){
	if( font_set.erase( f ) ) delete f;
}

//////////////
// 3D STUFF //
//////////////

#ifdef PRO

static int maxDevType;
/*
static HRESULT CALLBACK enumDevice( char *desc,char *name,D3DCAPS9 *devDesc,void *context ){//D3DDEVICEDESC7
	gxGraphics *g=(gxGraphics*)context;
	int t=0;
	GUID guid=devDesc->deviceGUID;
	if( guid==IID_IDirect3DRGBDevice ) t=1;
	else if( guid==IID_IDirect3DHALDevice ) t=2;
	else if( guid==IID_IDirect3DTnLHalDevice ) t=3;
	if( t>maxDevType ){
		g->dir3dDevDesc=*devDesc;
		maxDevType=t;
	}
	return D3DENUMRET_OK;
}*/

static HRESULT CALLBACK enumDevice(char* desc, char* name, D3DCAPS9* devDesc, void* context) {//D3DDEVICEDESC7
	PDIRECT3D9 d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (d3d) {
		D3DCAPS9 caps;
		if (SUCCEEDED(d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps))) {
			// 在这里处理设备信息
		}
		d3d->Release(); // 释放Direct3D对象
	}
}

static HRESULT CALLBACK enumZbuffFormat( LPDDPIXELFORMAT format,void *context ){
	gxGraphics *g=(gxGraphics*)context;
	if( format->dwZBufferBitDepth==g->primFmt.dwRGBBitCount ){
		g->zbuffFmt=*format;
		return D3DENUMRET_CANCEL;
	}
	if( format->dwZBufferBitDepth>g->zbuffFmt.dwZBufferBitDepth ){
		if( format->dwZBufferBitDepth<g->primFmt.dwRGBBitCount ){
			g->zbuffFmt=*format;
		}
	}
	return D3DENUMRET_OK;
}

struct TexFmt{
	DDPIXELFORMAT fmt;
	int bits,a_bits,rgb_bits;
};

static int cntBits( int mask ){
	int n=0;
	for( int k=0;k<32;++k ){
		if( mask & (1<<k) ) ++n;
	}
	return n;
}

static vector<TexFmt> tex_fmts;

static HRESULT CALLBACK enumTextureFormat( DDPIXELFORMAT *fmt,void *p ){
	TexFmt t;
	t.fmt=*fmt;
	t.bits=fmt->dwRGBBitCount;
	t.a_bits=(fmt->dwFlags & DDPF_ALPHAPIXELS) ? cntBits(fmt->dwRGBAlphaBitMask) : 0;
	t.rgb_bits=(fmt->dwFlags & DDPF_RGB) ? cntBits(fmt->dwRBitMask|fmt->dwGBitMask|fmt->dwBBitMask) : 0;

	tex_fmts.push_back( t );

	return D3DENUMRET_OK;
}

static string itobin( int n ){
	string t;
	for( int k=0;k<32;n<<=1,++k ){
		t+=(n&0x80000000) ? '1' : '0';
	}
	return t;
}

static void debugPF( const DDPIXELFORMAT &pf ){
	string t;
	t="Bits:"+itoa( pf.dwRGBBitCount );
	gx_runtime->debugLog( t.c_str() );
	t="R Mask:"+itobin( pf.dwRBitMask );
	gx_runtime->debugLog( t.c_str() );
	t="G Mask:"+itobin( pf.dwGBitMask );
	gx_runtime->debugLog( t.c_str() );
	t="B Mask:"+itobin( pf.dwBBitMask );
	gx_runtime->debugLog( t.c_str() );
	t="A Mask:"+itobin( pf.dwRGBAlphaBitMask );
	gx_runtime->debugLog( t.c_str() );
}

static void pickTexFmts( gxGraphics *g,int hi ){
	//texRGBFmt.
	{
		int pick=-1,max=0,bits;
		for( int d=g->primFmt.dwRGBBitCount;d<=32;d+=8 ){
			for( int k=0;k<tex_fmts.size();++k ){
				const TexFmt &t=tex_fmts[k];
				if( t.bits>d || !t.rgb_bits || t.rgb_bits<max ) continue;
				if( t.rgb_bits==max && t.bits>=bits ) continue;
				pick=k;max=t.rgb_bits;bits=t.bits;
			}
			if( !hi && pick>=0 ) break;
		}
		if( pick<0 ) g->texRGBFmt[hi]=g->primFmt;
		else g->texRGBFmt[hi]=tex_fmts[pick].fmt;
	}
	//texAlphaFmt
	{
		int pick=-1,max=0,bits;
		for( int d=g->primFmt.dwRGBBitCount;d<=32;d+=8 ){
			for( int k=0;k<tex_fmts.size();++k ){
				const TexFmt &t=tex_fmts[k];
				if( t.bits>d || !t.a_bits || t.a_bits<max ) continue;
				if( t.a_bits==max && t.bits>=bits ) continue;
				pick=k;max=t.a_bits;bits=t.bits;
			}
			if( !hi && pick>=0 ) break;
		}
		if( pick<0 ) g->texAlphaFmt[hi]=g->primFmt;
		else g->texAlphaFmt[hi]=tex_fmts[pick].fmt;
	}
	//texRGBAlphaFmt
	{
		int pick=-1,a8rgb8=-1,max=0,bits;
		for( int d=g->primFmt.dwRGBBitCount;d<=32;d+=8 ){
			for( int k=0;k<tex_fmts.size();++k ){
				const TexFmt &t=tex_fmts[k];
				if( t.a_bits==8 && t.bits==16 ){ a8rgb8=k;continue; }
				if( t.bits>d || !t.a_bits || !t.rgb_bits || t.a_bits<max ) continue;
				if( t.a_bits==max && t.bits>=bits ) continue;
				pick=k;max=t.a_bits;bits=t.bits;
			}
			if( !hi && pick>=0 ) break;
		}
		if( pick<0 ) pick=a8rgb8;
		if( pick<0 ) g->texRGBAlphaFmt[hi]=g->primFmt;
		else g->texRGBAlphaFmt[hi]=tex_fmts[pick].fmt;
	}
	//texRGBMaskFmt...
	{
		int pick=-1,max=0,bits;
		for( int d=g->primFmt.dwRGBBitCount;d<=32;d+=8 ){
			for( int k=0;k<tex_fmts.size();++k ){
				const TexFmt &t=tex_fmts[k];
				if( !t.a_bits || !t.rgb_bits || t.rgb_bits<max ) continue;
				if( t.rgb_bits==max && t.bits>=bits ) continue;
				pick=k;max=t.rgb_bits;bits=t.bits;
			}
			if( !hi && pick>=0 ) break;
		}
		if( pick<0 ) g->texRGBMaskFmt[hi]=g->primFmt;
		else g->texRGBMaskFmt[hi]=tex_fmts[pick].fmt;
	}
}
/*
gxScene *gxGraphics::createScene( int flags ){
	if( scene_set.size() ) return 0;

	//get d3d
	if( dirDraw->QueryInterface( IID_IDirect3D9,(void**)&dir3d )>=0 ){
		//enum devices
		maxDevType=0;
		if( dir3d->EnumDevices( enumDevice,this )>=0 && maxDevType>1 ){
			//enum zbuffer formats
			zbuffFmt.dwZBufferBitDepth=0;
			if( dir3d->EnumZBufferFormats( dir3dDevDesc.deviceGUID,enumZbuffFormat,this )>=0 ){
				//create zbuff for back buffer
				if( back_canvas->attachZBuffer() ){
					//create 3d device
					if( dir3d->CreateDevice( dir3dDevDesc.deviceGUID,back_canvas->getSurface(),&dir3dDev )>=0 ){
						//enum texture formats
						tex_fmts.clear();
						if( dir3dDev->EnumTextureFormats( enumTextureFormat,this )>=0 ){
							pickTexFmts( this,0 );
							pickTexFmts( this,1 );
							tex_fmts.clear();
#ifdef BETA
							gx_runtime->debugLog( "Texture RGB format:" );
							debugPF( texRGBFmt );
							gx_runtime->debugLog( "Texture Alpha format:" );
							debugPF( texAlphaFmt );
							gx_runtime->debugLog( "Texture RGB Alpha format:" );
							debugPF( texRGBAlphaFmt );
							gx_runtime->debugLog( "Texture RGB Mask format:" );
							debugPF( texRGBMaskFmt );
							gx_runtime->debugLog( "Texture Primary format:" );
							debugPF( primFmt );
							string ts="ZBuffer Bit Depth:"+itoa( zbuffFmt.dwZBufferBitDepth );
							gx_runtime->debugLog( ts.c_str() );
#endif
							gxScene *scene=d_new gxScene( this,back_canvas );
							scene_set.insert( scene );

							dummy_mesh=createMesh( 8,12,0 );

							return scene;
						}
						dir3dDev->Release();
						dir3dDev=0;
					}
					back_canvas->releaseZBuffer();
				}
			}
		}
		dir3d->Release();
		dir3d=0;
	}
	return 0;
}*/

gxScene* gxGraphics::createScene(int flags) {
	if (scene_set.size()) return 0;

	// 定义纹理宽度和高度
	int width = 256;
	int height = 256;

	//get d3d
	IDirect3D9* dir3d = 0;
	if (dirDraw->QueryInterface(IID_IDirect3D9, (void**)&dir3d) >= 0) {
		D3DADAPTER_IDENTIFIER9 adapterId;
		dir3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapterId);
		D3DPRESENT_PARAMETERS d3dpp;
		ZeroMemory(&d3dpp, sizeof(d3dpp));
		d3dpp.BackBufferWidth = 800;
		d3dpp.BackBufferHeight = 600;
		d3dpp.BackBufferCount = 1;
		d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
		d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
		d3dpp.MultiSampleQuality = 0;
		d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		d3dpp.hDeviceWindow = NULL;
		d3dpp.Windowed = TRUE;
		d3dpp.EnableAutoDepthStencil = TRUE;
		d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

		IDirect3DDevice9* dir3dDev = 0;
		if (dir3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &dir3dDev) >= 0) {
			IDirect3DSurface9* backBuffer = nullptr;
			dir3dDev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
			D3DSURFACE_DESC backbufferDesc;
			backBuffer->GetDesc(&backbufferDesc);
			D3DFORMAT zbuffFmt = D3DFMT_D16;
			IDirect3DSurface9* zBuffer = nullptr;
			dir3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, backbufferDesc.Format, D3DFMT_D24S8, zbuffFmt);
			dir3dDev->CreateDepthStencilSurface(800, 600, zbuffFmt, backbufferDesc.MultiSampleType, backbufferDesc.MultiSampleQuality, FALSE, &zBuffer, nullptr);
			dir3dDev->SetDepthStencilSurface(zBuffer);

			// Create texture
			IDirect3DTexture9* texture = nullptr;
			if (SUCCEEDED(dir3dDev->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, NULL))) {
				gxScene* scene = new gxScene(this, back_canvas);
				scene_set.insert(scene);

				dummy_mesh = createMesh(8, 12, 0);

				return scene;
			}

			zBuffer->Release();
			backBuffer->Release();
			dir3dDev->Release();
			dir3dDev = 0;
		}
		dir3d->Release();
		dir3d = 0;
	}
	return 0;
}

gxScene *gxGraphics::verifyScene( gxScene *s ){
	return scene_set.count( s ) ? s : 0;
}

void gxGraphics::freeScene( gxScene *scene ){
	if( !scene_set.erase( scene ) ) return;
	dummy_mesh=0;
	while( mesh_set.size() ) freeMesh( *mesh_set.begin() );
	back_canvas->releaseZBuffer();
	if( dir3dDev ){ dir3dDev->Release();dir3dDev=0; }
	if( dir3d ){ dir3d->Release();dir3d=0; }
	delete scene;
}
/*
gxMesh *gxGraphics::createMesh( int max_verts,int max_tris,int flags ){

	static const int VTXFMT=
	D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX2|
	D3DFVF_TEXCOORDSIZE2(0)|D3DFVF_TEXCOORDSIZE2(1);

	int vbflags=0;

	//XP or less?
	if( runtime->osinfo.dwMajorVersion<6 ){
		vbflags|=D3DVBCAPS_WRITEONLY;
	}

	D3DVERTEXBUFFERDESC desc={ sizeof(desc),vbflags,VTXFMT,max_verts };

	IDirect3DVertexBuffer9 *buff;
	if( dir3d->CreateVertexBuffer( &desc,&buff,0 )<0 ) return 0;
	WORD *indices=d_new WORD[max_tris*3];
	gxMesh *mesh=d_new gxMesh( this,buff,indices,max_verts,max_tris );
	mesh_set.insert( mesh );
	return mesh;
}*/

gxMesh* gxGraphics::createMesh(int max_verts, int max_tris, int flags) {
	struct VertexType {
		D3DXVECTOR3 position;  // 顶点位置
		D3DXVECTOR3 normal;    // 顶点法线
		D3DCOLOR diffuse;     // 顶点颜色
		float tu, tv;          // 纹理坐标
	};
	static const int VTXFMT =
		D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX2 |
		D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE2(1);

	int vbflags = 0;

	//XP or less?
	if (runtime->osinfo.dwMajorVersion < 6) {
		vbflags |= D3DVBCAPS_WRITEONLY;
	}

	// 创建顶点缓冲区
	IDirect3DDevice9* dir3dDev = 0;
	if (dirDraw->QueryInterface(IID_IDirect3DDevice9, (void**)&dir3dDev) >= 0) {
		IDirect3DVertexBuffer9* buff;
		if (dir3dDev->CreateVertexBuffer(max_verts * sizeof(VertexType), D3DUSAGE_WRITEONLY, VTXFMT, D3DPOOL_MANAGED, &buff, NULL) < 0) {
			dir3dDev->Release();
			return 0;
		}

		WORD* indices = d_new WORD[max_tris * 3];
		gxMesh* mesh = d_new gxMesh(this, buff, indices, max_verts, max_tris);
		mesh_set.insert(mesh);

		dir3dDev->Release();
		return mesh;
	}

	return 0;
}

gxMesh *gxGraphics::verifyMesh( gxMesh *m ){
	return mesh_set.count( m ) ? m : 0;
}

void gxGraphics::freeMesh( gxMesh *mesh ){
	if( mesh_set.erase( mesh ) ) delete mesh;
}

#endif
