#include "Kobayashi.h"

using namespace std;
using namespace DirectX;

Kobayashi::Kobayashi(int x, int y, float timeStep)
{
	_objectCount = { x, y };

	_dx = 0.03f;
	_dy = 0.03f;
	_dt = timeStep;

	_initialize();
}

Kobayashi::~Kobayashi()
{
}

void Kobayashi::_initialize()
{
	//
	_tau = _tauValue;
	_epsilonBar = _epsilonBarValue;		// Mean of epsilon. scaling factor that determines how much the microscopic front is magnified
	_mu = _muValue;
	_K = _KValue;						// Latent heat 
	_delta = _deltaValue;				// Strength of anisotropy (speed of growth in preferred directions)
	_anisotropy = _anisotropyValue;		// Degree of anisotropy
	_alpha = _alphaValue;
	_gamma = _gammaValue;
	_tEq = _tEqValue;
	//

	size_t vSize = static_cast<size_t>(_objectCount.x) * static_cast<size_t>(_objectCount.y);
	_phi.assign(vSize, 0.0f);
	_t.assign(vSize, 0.0f);
	_gradPhiX.assign(vSize, 0.0f);
	_gradPhiY.assign(vSize, 0.0f);
	_lapPhi.assign(vSize, 0.0f);
	_lapT.assign(vSize, 0.0f);
	_angl.assign(vSize, 0.0f);
	_epsilon.assign(vSize, 0.0f);
	_epsilonDeriv.assign(vSize, 0.0f);


	// Create the neuclei
	_createNucleus(_objectCount.x / 2, _objectCount.y / 2);
}

void Kobayashi::_createNucleus(int x, int y)
{
	_phi[_INDEX(x, y)] = 1.0f;
	_phi[_INDEX(x - 1, y)] = 1.0f;
	_phi[_INDEX(x + 1, y)] = 1.0f;
	_phi[_INDEX(x, y - 1)] = 1.0f;
	_phi[_INDEX(x, y + 1)] = 1.0f;
}

void Kobayashi::_computeGradientLaplacian()
{

	for (int j = 0; j < _objectCount.y; j++)
	{
		for (int i = 0; i < _objectCount.x; i++)
		{

			int i_plus = (i + 1) % _objectCount.x;
			int i_minus = ((i - 1) + _objectCount.x) % _objectCount.x;
			int j_plus = (j + 1) % _objectCount.y;
			int j_minus = ((j - 1) + _objectCount.y) % _objectCount.y;


			_gradPhiX[_INDEX(i, j)] = (_phi[_INDEX(i_plus, j)] - _phi[_INDEX(i_minus, j)]) / _dx;
			_gradPhiY[_INDEX(i, j)] = (_phi[_INDEX(i, j_plus)] - _phi[_INDEX(i, j_minus)]) / _dy;

			_lapPhi[_INDEX(i, j)] = 
				(2.0f * (_phi[_INDEX(i_plus, j)] + _phi[_INDEX(i_minus, j)] + _phi[_INDEX(i, j_plus)] + _phi[_INDEX(i, j_minus)])
				+ _phi[_INDEX(i_plus, j_plus)] + _phi[_INDEX(i_minus, j_minus)] + _phi[_INDEX(i_minus, j_plus)] + _phi[_INDEX(i_plus, j_minus)]
				- 12.0f * _phi[_INDEX(i, j)])
				/ (3.0f * _dx * _dx);
			_lapT[_INDEX(i, j)] = 
				(2.0f * (_t[_INDEX(i_plus, j)] + _t[_INDEX(i_minus, j)] + _t[_INDEX(i, j_plus)] + _t[_INDEX(i, j_minus)])
				+ _t[_INDEX(i_plus, j_plus)] + _t[_INDEX(i_minus, j_minus)] + _t[_INDEX(i_minus, j_plus)] + _t[_INDEX(i_plus, j_minus)]
				- 12.0f * _t[_INDEX(i, j)])
				/ (3.0f * _dx * _dx);


			if (_gradPhiX[_INDEX(i, j)] <= +EPS_F && _gradPhiX[_INDEX(i, j)] >= -EPS_F) // _gradPhiX[i][j] == 0.0f
				if (_gradPhiY[_INDEX(i, j)] < -EPS_F)
					_angl[_INDEX(i, j)] = -0.5f * PI_F;
				else if (_gradPhiY[_INDEX(i, j)] > +EPS_F)
					_angl[_INDEX(i, j)] = 0.5f * PI_F;

			if (_gradPhiX[_INDEX(i, j)] > +EPS_F)
				if (_gradPhiY[_INDEX(i, j)] < -EPS_F)
					_angl[_INDEX(i, j)] = 2.0f * PI_F + atan(_gradPhiY[_INDEX(i, j)] / _gradPhiX[_INDEX(i, j)]);
				else if (_gradPhiY[_INDEX(i, j)] > +EPS_F)
					_angl[_INDEX(i, j)] = atan(_gradPhiY[_INDEX(i, j)] / _gradPhiX[_INDEX(i, j)]);

			if (_gradPhiX[_INDEX(i, j)] < -EPS_F)
				_angl[_INDEX(i, j)] = PI_F + atan(_gradPhiY[_INDEX(i, j)] / _gradPhiX[_INDEX(i, j)]);

			
			_epsilon[_INDEX(i, j)] = _epsilonBar * (1.0f + _delta * cos(_anisotropy * _angl[_INDEX(i, j)]));
			_epsilonDeriv[_INDEX(i, j)] = -_epsilonBar * _anisotropy * _delta * sin(_anisotropy * _angl[_INDEX(i, j)]);

		}
	}
}

void Kobayashi::_evolution()
{
	for (int j = 0; j < _objectCount.y; j++)
	{
		for (int i = 0; i < _objectCount.x; i++)
		{

			int i_plus = (i + 1) % _objectCount.x;
			int i_minus = ((i - 1) + _objectCount.x) % _objectCount.x;
			int j_plus = (j + 1) % _objectCount.y;
			int j_minus = ((j - 1) + _objectCount.y) % _objectCount.y;


			float gradEpsPowX = 
				(_epsilon[_INDEX(i_plus, j)] * _epsilon[_INDEX(i_plus, j)] 
					- _epsilon[_INDEX(i_minus, j)] * _epsilon[_INDEX(i_minus, j)]) / _dx;
			float gradEpsPowY = 
				(_epsilon[_INDEX(i, j_plus)] * _epsilon[_INDEX(i, j_plus)] 
					- _epsilon[_INDEX(i, j_minus)] * _epsilon[_INDEX(i, j_minus)]) / _dy;

			float term1 = (_epsilon[_INDEX(i, j_plus)] * _epsilonDeriv[_INDEX(i, j_plus)] * _gradPhiX[_INDEX(i, j_plus)]
				- _epsilon[_INDEX(i, j_minus)] * _epsilonDeriv[_INDEX(i, j_minus)] * _gradPhiX[_INDEX(i, j_minus)])
				/ _dy;

			float term2 = -(_epsilon[_INDEX(i_plus, j)] * _epsilonDeriv[_INDEX(i_plus, j)] * _gradPhiY[_INDEX(i_plus, j)]
				- _epsilon[_INDEX(i_minus, j)] * _epsilonDeriv[_INDEX(i_minus, j)] * _gradPhiY[_INDEX(i_minus, j)])
				/ _dx;
			float term3 = gradEpsPowX * _gradPhiX[_INDEX(i, j)] + gradEpsPowY * _gradPhiY[_INDEX(i, j)];

			float m = _alpha / PI_F * atan(_gamma*(_tEq - _t[_INDEX(i, j)]));

			float oldPhi = _phi[_INDEX(i, j)];
			float oldT = _t[_INDEX(i, j)];

			_phi[_INDEX(i, j)] = _phi[_INDEX(i, j)] +
				(term1 + term2 + _epsilon[_INDEX(i, j)] * _epsilon[_INDEX(i, j)] * _lapPhi[_INDEX(i, j)]
					+ term3
					+ oldPhi * (1.0f - oldPhi)*(oldPhi - 0.5f + m))*_dt / _tau;
			_t[_INDEX(i, j)] = oldT + _lapT[_INDEX(i, j)] * _dt + _K * (_phi[_INDEX(i, j)] - oldPhi);


		}

	}
}

wchar_t* Kobayashi::_int2wchar(int value)
{
	_itow(value, wBuffer, 10);
	return wBuffer;
}


#pragma region implementation
// ################################## implementation ####################################
// Simulation methods
void Kobayashi::iUpdate()
{
	clock_t startTime = clock();
	for (int i = 0; i < 10; i++)
	{
		_computeGradientLaplacian();
		_evolution();
	}
	clock_t endTime = clock();

	_simTime += endTime - startTime; // ms
	_simFrame++;
}

void Kobayashi::iResetSimulationState(std::vector<ConstantBuffer>& constantBuffer)
{
	_initialize();

	_dxapp->update();
	_dxapp->draw();
	_simTime = 0;
	_simFrame = 0;
}


// Mesh methods
std::vector<Vertex>& Kobayashi::iGetVertice()
{
	_vertices.clear();

	_vertices.push_back(Vertex({ DirectX::XMFLOAT3(-0.5f, -0.5f, -0.0f) }));
	_vertices.push_back(Vertex({ DirectX::XMFLOAT3(-0.5f, +0.5f, -0.0f) }));
	_vertices.push_back(Vertex({ DirectX::XMFLOAT3(+0.5f, +0.5f, -0.0f) }));
	_vertices.push_back(Vertex({ DirectX::XMFLOAT3(+0.5f, -0.5f, -0.0f) }));

	return _vertices;
}

std::vector<unsigned int>& Kobayashi::iGetIndice()
{
	_indices.clear();

	_indices.push_back(0); _indices.push_back(1); _indices.push_back(2);
	_indices.push_back(0); _indices.push_back(2); _indices.push_back(3);

	return _indices;
}

UINT Kobayashi::iGetVertexBufferSize()
{
	return 4;
}

UINT Kobayashi::iGetIndexBufferSize()
{
	return 6;
}


// DirectX methods
void Kobayashi::iCreateObject(std::vector<ConstantBuffer>& constantBuffer)
{
	for (int j = 0; j < _objectCount.y; j++)
	{
		for (int i = 0; i < _objectCount.x; i++)
		{
			// Position
			XMFLOAT2 pos = XMFLOAT2(
				(float)i,
				(float)j);

			struct ConstantBuffer objectCB;
			objectCB.world = DXViewer::util::transformMatrix(pos.x, pos.y, 0.0f, 1.0f);
			objectCB.worldViewProj = DXViewer::util::transformMatrix(0.0f, 0.0f, 0.0f);
			objectCB.transInvWorld = DXViewer::util::transformMatrix(0.0f, 0.0f, 0.0f);
			objectCB.color = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

			constantBuffer.push_back(objectCB);
		}
	}
}

void Kobayashi::iUpdateConstantBuffer(std::vector<ConstantBuffer>& constantBuffer, int i)
{
	int size = constantBuffer.size();
	int j = i / (int)(sqrt(size));
	int k = i % (int)(sqrt(size));

	float phi = _phi[_INDEX(j, k)];
	XMFLOAT4 color = {0.0f, 0.0f, 0.0f, 1.0f};

	if (phi > 0.1f && phi <= 0.5f)
		color = { 0.2f, 0.95f, 0.9f, 1.0f };
	else if (phi > 0.5f)
		color = { phi, phi, phi, 1.0f };

	constantBuffer[i].color = color;
}

void Kobayashi::iDraw(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList, int size, UINT indexCount, int i)
{
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Kobayashi::iSetDXApp(DX12App* dxApp)
{
	_dxapp = dxApp;
}

UINT Kobayashi::iGetConstantBufferSize()
{
	return _objectCount.x * _objectCount.y * 2;
}

XMINT3 Kobayashi::iGetObjectCount()
{
	return { _objectCount.x, _objectCount.y, 0 };
}

XMFLOAT3 Kobayashi::iGetObjectSize()
{
	return { 1.0f, 1.0f, 0.0f };
}

XMFLOAT3 Kobayashi::iGetObjectPositionOffset()
{
	return { 0.0f, 0.0f, 0.0f };
}

bool Kobayashi::iIsUpdated()
{
	return _updateFlag;
}

void Kobayashi::iWMCreate(HWND hwnd, HINSTANCE hInstance)
{
	CreateWindow(L"button", _updateFlag ? L"��" : L"��", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		65, 290, 50, 25, hwnd, reinterpret_cast<HMENU>(_COM::PLAY), hInstance, NULL);
	CreateWindow(L"button", L"��", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		115, 290, 50, 25, hwnd, reinterpret_cast<HMENU>(_COM::STOP), hInstance, NULL);
	CreateWindow(L"button", L"��l", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		165, 290, 50, 25, hwnd, reinterpret_cast<HMENU>(_COM::NEXTSTEP), hInstance, NULL);

	CreateWindow(L"static", L"time :", WS_CHILD | WS_VISIBLE,
		95, 340, 40, 20, hwnd, reinterpret_cast<HMENU>(-1), hInstance, NULL);
	CreateWindow(L"static", _int2wchar(_simTime), WS_CHILD | WS_VISIBLE,
		140, 340, 40, 20, hwnd, reinterpret_cast<HMENU>(_COM::TIME_TEXT), hInstance, NULL);
	CreateWindow(L"static", L"frame :", WS_CHILD | WS_VISIBLE,
		86, 360, 45, 20, hwnd, reinterpret_cast<HMENU>(-1), hInstance, NULL);
	CreateWindow(L"static", _int2wchar(_simFrame), WS_CHILD | WS_VISIBLE,
		140, 360, 40, 20, hwnd, reinterpret_cast<HMENU>(_COM::FRAME_TEXT), hInstance, NULL);


	CreateWindow(L"static", L"tau :", WS_CHILD | WS_VISIBLE,
		80, 220, 20, 20, hwnd, reinterpret_cast<HMENU>(-1), hInstance, NULL);
	CreateWindow(L"static", L"0.0003", WS_CHILD | WS_VISIBLE,
		105, 220, 50, 20, hwnd, reinterpret_cast<HMENU>(-1), hInstance, NULL);
	HWND tauScroll =
		CreateWindow(L"scrollbar", NULL, WS_CHILD | WS_VISIBLE | SBS_HORZ,
			167, 220, 100, 20, hwnd, reinterpret_cast<HMENU>(_COM::ANISO_BAR), hInstance, NULL);

	CreateWindow(L"static", L"anisotropy :", WS_CHILD | WS_VISIBLE,
		20, 250, 80, 20, hwnd, reinterpret_cast<HMENU>(-1), hInstance, NULL);
	CreateWindow(L"static", L"", WS_CHILD | WS_VISIBLE,
		105, 250, 50, 20, hwnd, reinterpret_cast<HMENU>(_COM::ANISO_VALUE), hInstance, NULL);
	HWND anisotropyScroll =
		CreateWindow(L"scrollbar", NULL, WS_CHILD | WS_VISIBLE | SBS_HORZ,
			167, 250, 100, 20, hwnd, reinterpret_cast<HMENU>(_COM::ANISO_BAR), hInstance, NULL);

	if (_updateFlag)
	{
		EnableWindow(GetDlgItem(hwnd, static_cast<int>(_COM::NEXTSTEP)), false);
	}


	SetScrollRange(anisotropyScroll, SB_CTL, 2, 8, TRUE);
	SetScrollPos(anisotropyScroll, SB_CTL, _scrollPos, TRUE);

	SetTimer(hwnd, 1, 10, NULL);
}

void Kobayashi::iWMCommand(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, HINSTANCE hInstance)
{
	switch (LOWORD(wParam))
	{
		// ### Execution buttons ###
		case static_cast<int>(_COM::PLAY):
		{
			_updateFlag = !_updateFlag;
			SetDlgItemText(hwnd, static_cast<int>(_COM::PLAY), _updateFlag ? L"��" : L"��");

			EnableWindow(GetDlgItem(hwnd, static_cast<int>(_COM::STOP)), true);
			EnableWindow(GetDlgItem(hwnd, static_cast<int>(_COM::NEXTSTEP)), !_updateFlag);
		}
		break;
		case static_cast<int>(_COM::STOP):
		{
			_dxapp->resetSimulationState();
		}
		break;
		case static_cast<int>(_COM::NEXTSTEP):
		{
			iUpdate();
			_dxapp->update();
			_dxapp->draw();
		}
		break;
	// #####################
	}
}

void Kobayashi::iWMHScroll(HWND hwnd, WPARAM wParam, LPARAM lParam, HINSTANCE hInstance)
{
	switch (LOWORD(wParam))
	{
	case SB_THUMBTRACK:
		_scrollPos = HIWORD(wParam);
		break;

	case SB_LINELEFT:
		_scrollPos = max(2, _scrollPos - 1);
		break;

	case SB_LINERIGHT:
		_scrollPos = min(8, _scrollPos + 1);
		break;

	case SB_PAGELEFT:
		_scrollPos = max(2, _scrollPos - 5);
		break;

	case SB_PAGERIGHT:
		_scrollPos = min(8, _scrollPos + 5);
		break;
	}

	SetScrollPos((HWND)lParam, SB_CTL, _scrollPos, TRUE);
	SetDlgItemText(hwnd, static_cast<int>(_COM::ANISO_VALUE), _int2wchar(_scrollPos));

	_dxapp->resetSimulationState();
}

void Kobayashi::iWMTimer(HWND hwnd)
{
	SetDlgItemText(hwnd, static_cast<int>(_COM::TIME_TEXT), _int2wchar(_simTime));
	SetDlgItemText(hwnd, static_cast<int>(_COM::FRAME_TEXT), _int2wchar(_simFrame));
}

void Kobayashi::iWMDestory(HWND hwnd)
{
	KillTimer(hwnd, 1);
}

// #######################################################################################
#pragma endregion