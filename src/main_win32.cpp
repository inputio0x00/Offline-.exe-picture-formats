#include "ConversionJob.h"
#include "WicImageConverter.h"

#include <Windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr int IdDropList = 1001;
constexpr int IdTarget = 1002;
constexpr int IdConvert = 1003;
constexpr int IdQuality = 1004;
constexpr int IdStatus = 1005;
constexpr int IdOutput = 1006;

struct AppState {
    HWND list = nullptr;
    HWND target = nullptr;
    HWND quality = nullptr;
    HWND output = nullptr;
    HWND status = nullptr;
    std::vector<std::filesystem::path> files;
};

AppState* state(HWND hwnd) {
    return reinterpret_cast<AppState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

void setFont(HWND hwnd, HFONT font) {
    SendMessage(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

void addFile(HWND hwnd, const std::filesystem::path& path) {
    auto* app = state(hwnd);
    if (!app || !oic::formatFromExtension(path.wstring())) {
        return;
    }
    app->files.push_back(path);
    SendMessage(app->list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(path.c_str()));
}

void updateStatus(HWND hwnd, const std::wstring& text) {
    auto* app = state(hwnd);
    if (app && app->status) {
        SetWindowText(app->status, text.c_str());
    }
}

oic::ImageFormat selectedFormat(HWND hwnd) {
    auto* app = state(hwnd);
    auto index = static_cast<int>(SendMessage(app->target, CB_GETCURSEL, 0, 0));
    auto formats = oic::supportedFormats();
    if (index < 0 || index >= static_cast<int>(formats.size())) {
        return oic::ImageFormat::Png;
    }
    return formats[static_cast<std::size_t>(index)];
}

void convert(HWND hwnd) {
    auto* app = state(hwnd);
    if (!app || app->files.empty()) {
        updateStatus(hwnd, L"Add images first by dragging them into the file list.");
        return;
    }

    wchar_t outputText[MAX_PATH]{};
    GetWindowText(app->output, outputText, MAX_PATH);

    oic::ConversionOptions options;
    options.targetFormat = selectedFormat(hwnd);
    options.outputDirectory = outputText[0] ? std::filesystem::path(outputText) : std::filesystem::path(L"converted");
    options.quality = static_cast<int>(SendMessage(app->quality, TBM_GETPOS, 0, 0));

    auto plan = oic::buildPlan(app->files, options);
    try {
        oic::WicImageConverter converter;
        if (!converter.canEncode(options.targetFormat)) {
            updateStatus(hwnd, L"The selected encoder is unavailable. Install/bundle the corresponding codec and try again.");
            return;
        }
        converter.convertBatch(plan, options, [&](const oic::ConversionProgress& progress) {
            std::wstringstream message;
            message << L"Converted " << progress.completed << L" of " << progress.total;
            if (!progress.currentFile.empty()) {
                message << L" — " << progress.currentFile;
            }
            updateStatus(hwnd, message.str());
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        });
        updateStatus(hwnd, L"Conversion complete. Outputs are in the selected output folder.");
    } catch (const std::exception&) {
        updateStatus(hwnd, L"Conversion failed. Verify the source format is decodable and the output folder is writable.");
    }
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        auto* app = new AppState();
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        DragAcceptFiles(hwnd, TRUE);

        auto font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        CreateWindowEx(0, L"STATIC", L"Offline Image Converter", WS_CHILD | WS_VISIBLE,
                       24, 18, 420, 28, hwnd, nullptr, nullptr, nullptr);
        CreateWindowEx(0, L"STATIC", L"Drop HEIF, JPG, PNG, WEBP, or TIFF files below. Pick one target format and convert the whole batch offline.",
                       WS_CHILD | WS_VISIBLE, 24, 48, 760, 24, hwnd, nullptr, nullptr, nullptr);

        app->list = CreateWindowEx(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                                   24, 84, 760, 250, hwnd, reinterpret_cast<HMENU>(IdDropList), nullptr, nullptr);
        app->target = CreateWindowEx(0, WC_COMBOBOX, nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                     24, 360, 180, 180, hwnd, reinterpret_cast<HMENU>(IdTarget), nullptr, nullptr);
        for (auto format : oic::supportedFormats()) {
            auto name = oic::toDisplayName(format);
            SendMessage(app->target, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
        }
        SendMessage(app->target, CB_SETCURSEL, 2, 0);

        app->quality = CreateWindowEx(0, TRACKBAR_CLASS, nullptr, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                      230, 360, 180, 36, hwnd, reinterpret_cast<HMENU>(IdQuality), nullptr, nullptr);
        SendMessage(app->quality, TBM_SETRANGE, TRUE, MAKELPARAM(1, 100));
        SendMessage(app->quality, TBM_SETPOS, TRUE, 90);

        app->output = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"converted", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                     430, 360, 220, 26, hwnd, reinterpret_cast<HMENU>(IdOutput), nullptr, nullptr);
        CreateWindowEx(0, L"BUTTON", L"Convert batch", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                       670, 358, 114, 30, hwnd, reinterpret_cast<HMENU>(IdConvert), nullptr, nullptr);
        app->status = CreateWindowEx(0, L"STATIC", L"Ready. Drag files into the list to begin.", WS_CHILD | WS_VISIBLE,
                                     24, 410, 760, 24, hwnd, reinterpret_cast<HMENU>(IdStatus), nullptr, nullptr);

        EnumChildWindows(hwnd, [](HWND child, LPARAM param) -> BOOL {
            setFont(child, reinterpret_cast<HFONT>(param));
            return TRUE;
        }, reinterpret_cast<LPARAM>(font));
        return 0;
    }
    case WM_DROPFILES: {
        HDROP drop = reinterpret_cast<HDROP>(wParam);
        const UINT count = DragQueryFile(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; ++i) {
            wchar_t path[MAX_PATH]{};
            DragQueryFile(drop, i, path, MAX_PATH);
            addFile(hwnd, path);
        }
        DragFinish(drop);
        std::wstringstream message;
        message << state(hwnd)->files.size() << L" supported file(s) queued.";
        updateStatus(hwnd, message.str());
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IdConvert) {
            convert(hwnd);
            return 0;
        }
        break;
    case WM_DESTROY:
        delete state(hwnd);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    INITCOMMONCONTROLSEX controls{sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASS windowClass{};
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"OfflineImageConverterWindow";
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = CreateSolidBrush(RGB(245, 247, 250));
    RegisterClass(&windowClass);

    HWND hwnd = CreateWindowEx(0, windowClass.lpszClassName, L"Offline Image Converter",
                               WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
                               CW_USEDEFAULT, CW_USEDEFAULT, 830, 500,
                               nullptr, nullptr, instance, nullptr);
    if (!hwnd) {
        return 1;
    }
    ShowWindow(hwnd, showCommand);

    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    return static_cast<int>(message.wParam);
}
