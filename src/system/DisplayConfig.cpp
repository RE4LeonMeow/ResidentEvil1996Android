// DisplayConfig.cpp - Display mode enumeration and configuration
// Functions: CheckVideoCapabilities, EnumerateAndSelectDisplayMode,
//            GetDisplayModeCount (0x00448770), GetDisplayModeRect (0x004487a0),
//            EnumDisplayModesCallback (0x00442930)
// Adapted from Ghidra decompilation - modernized from DirectDraw to DXGI
#include "Globals.h"
#include <d3d11.h>
#include <dxgi.h>

// ============================================================================
// CheckVideoCapabilities - Check what video adapters are available (various addresses)
// ============================================================================
int CheckVideoCapabilities(void)
{
    // The original checked DirectDraw capabilities per adapter
    // Modern adaptation: enumerate DXGI adapters
    
    IDXGIFactory* pFactory = NULL;
    HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);
    if (FAILED(hr)) {
        return 0; // Software rendering
    }
    
    int adapterIndex = 0;
    IDXGIAdapter* pAdapter = NULL;
    
    while (pFactory->EnumAdapters(adapterIndex, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC desc;
        pAdapter->GetDesc(&desc);
        pAdapter->Release();
        
        // In original, different adapters map to different indices
        // We map modern adapters: 0=primary, 1+=additional
        adapterIndex++;
    }
    
    pFactory->Release();
    
    // Return highest adapter index found (original logic)
    // 0 = software, 1+ = hardware adapters
    return (adapterIndex > 0) ? adapterIndex - 1 : 0;
}

// ============================================================================
// EnumerateAndSelectDisplayMode - Enumerate modes and get selection (0x00442870)
// ============================================================================
INT_PTR EnumerateAndSelectDisplayMode(void)
{
    // The original used DirectDraw EnumerateDisplayModes
    // Modern adaptation: use DXGI mode enumeration
    
    IDXGIFactory* pFactory = NULL;
    HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);
    if (FAILED(hr)) {
        return -1;
    }
    
    IDXGIAdapter* pAdapter = NULL;
    hr = pFactory->EnumAdapters(g_dwSelectedDisplayAdapterID, &pAdapter);
    if (FAILED(hr)) {
        pFactory->Release();
        return -1;
    }
    
    IDXGIOutput* pOutput = NULL;
    hr = pAdapter->EnumOutputs(0, &pOutput);
    if (FAILED(hr)) {
        pAdapter->Release();
        pFactory->Release();
        return -1;
    }
    
    UINT numModes = 0;
    hr = pOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &numModes, NULL);
    
    if (SUCCEEDED(hr) && numModes > 0) {
        DXGI_MODE_DESC* modes = new DXGI_MODE_DESC[numModes];
        hr = pOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &numModes, modes);
        
        if (SUCCEEDED(hr)) {
            g_NumDisplayModes = 0;
            
            for (UINT i = 0; i < numModes && g_NumDisplayModes < MAX_DISPLAY_MODES; i++) {
                // Only accept 16bpp+ modes
                // DXGI doesn't report BPP directly in the same way, use format
                if (modes[i].Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
                    modes[i].Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                    modes[i].Format == DXGI_FORMAT_B8G8R8X8_UNORM) {
                    
                    g_DisplayModeBuffer[g_NumDisplayModes].dwWidth = modes[i].Width;
                    g_DisplayModeBuffer[g_NumDisplayModes].dwHeight = modes[i].Height;
                    g_DisplayModeBuffer[g_NumDisplayModes].dwBPP = 32;
                    g_DisplayModeBuffer[g_NumDisplayModes].dwRefreshRate = modes[i].RefreshRate.Numerator / (modes[i].RefreshRate.Denominator ? modes[i].RefreshRate.Denominator : 1);
                    g_DisplayModeBuffer[g_NumDisplayModes].dwFlags = 0;
                    g_NumDisplayModes++;
                }
            }
        }
        
        delete[] modes;
    }
    
    pOutput->Release();
    pAdapter->Release();
    pFactory->Release();
    
    // Return first valid fullscreen mode as default (640x480 if available)
    // Original returned dialog result; we return mode index or 0
    if (g_NumDisplayModes > 0) {
        return 0; // Default to first mode
    }
    
    return 0; // Windowed default
}

// ============================================================================
// GetDisplayModeCount - Get number of available display modes (0x00448770)
// ============================================================================
int GetDisplayModeCount(void)
{
    return g_NumDisplayModes;
}

// ============================================================================
// GetDisplayModeRect - Get display mode info by index (0x004487a0)
// ============================================================================
void GetDisplayModeRect(int modeIndex, DWORD* pRect)
{
    if (modeIndex >= 0 && modeIndex < g_NumDisplayModes) {
        pRect[0] = g_DisplayModeBuffer[modeIndex].dwWidth;
        pRect[1] = g_DisplayModeBuffer[modeIndex].dwHeight;
        pRect[2] = g_DisplayModeBuffer[modeIndex].dwBPP;
        pRect[3] = g_DisplayModeBuffer[modeIndex].dwRefreshRate;
        pRect[4] = g_DisplayModeBuffer[modeIndex].dwFlags;
    } else {
        // Default 640x480@16bpp
        pRect[0] = 640;
        pRect[1] = 480;
        pRect[2] = 16;
        pRect[3] = 60;
        pRect[4] = 0;
    }
}
