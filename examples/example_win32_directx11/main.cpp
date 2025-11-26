/*

--- IGNORE ---
E-LIBRARY  ESPECIALLY MADE FOR FEU
FINAL PROJECT FOR COMPUTER PROGRAMMING 1
TEACHER - MS. HAZEL SAN LOVEREZ

MADE WITH PURE C++ AND IMGUI LIBRARY (COMPILED WITH C++17 STANDARD)
AI USED FOR COMMENTS AND DOCUMENTATION ONLY
- CLAUDE
- GPT5

INSTALLER USED FOR SETUP IS NSIS (NULLSOFT SCRIPTABLE INSTALL SYSTEM)

*/  


#include "../../imgui.h"
#include "../../backends/imgui_impl_win32.h"
#include "../../backends/imgui_impl_dx11.h"
#include "../../imgui_internal.h"
#include <windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcommon.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <ctime>
#include <algorithm>
#include <filesystem>
#include <direct.h>
#include <map>
#include <tuple>
#include <iomanip> 
#include <unordered_set>
#include <unordered_map>
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
// Uses stb_image to load PNG/JPG book covers.

// ---------------- DirectX Globals ----------------
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;

// ---------------- Structs ----------------
struct UserProfile {
    std::string username;
    std::string role;
    std::string password = "";
    std::string name = "N/A";
    std::string section = "N/A";
    std::string gender = "N/A";
    int age = 0;
};
struct Book {
    std::string title;
    std::string author;
    std::string category;
    std::string iconPath;
};

struct HistoryEntry {
    std::string bookTitle;
    std::string borrowerName;
    std::string borrowDate;
    std::string dueDate = "";  // Due date for return
    std::string returnDate = "";
    bool isReturned = false;
    std::string returnCode = "";
    double fine = 0.0;  // Fine amount in currency
    bool finePaid = false;  // Whether fine has been paid
};

struct ActivityLog {
    std::string timestamp;      // Date and time of activity
    std::string username;        // User who performed the action
    std::string action;          // Type of action (Login, Logout, Borrow, Return, etc.)
    std::string details;         // Additional details (book title, etc.)
    std::string ipAddress = "";  // Optional: track IP if needed
};

enum class BookDiffType { Added, Removed, Updated };

struct BookDiffEntry {
    Book currentBook;
    Book incomingBook;
    BookDiffType type = BookDiffType::Added;
};

struct BookDiffResult {
    int added = 0;
    int removed = 0;
    int updated = 0;
    std::vector<BookDiffEntry> entries;
};

// ---------------- Global State ----------------
std::vector<HistoryEntry> borrowHistory;
std::vector<ActivityLog> activityLogs;
std::vector<Book> books;
std::map<std::string, UserProfile> users;
std::unordered_map<std::string, std::unordered_set<std::string>> userFavorites;
UserProfile currentUser;
std::vector<std::string> categories = { "All" };
static UserProfile profileViewUser;
static std::string loginError = "";
static bool showProfilePopup = false;
static bool showBookDetailsPopup = false;
static std::string selectedBookTitle = "";

// Account creation state
static bool showCreateAccount = false;
static char newUsername[64] = "";
static char newPassword[64] = "";
static char newPasswordConfirm[64] = "";
static char newFullName[128] = "";
static char newSection[64] = "";
static char newGender[32] = "";
static int newAge = 0;
static std::string createAccountMessage = "";

// -- Force profile data reload on new user login --
static bool forceProfileBufferReload = true;

// -- Buffers for profile input --
static char profileNameBuffer[128] = "";
static char profileSectionBuffer[64] = "";
static char profileGenderBuffer[32] = "";
static int profileAge = 0;
static std::string profileMessage = "";
// Global shared library search buffer used by Student and Admin views
static char searchLibraryGlobal[128] = "";

// Book cover textures (DX11 shader resource views)
static std::map<std::string, ID3D11ShaderResourceView*> g_bookTextures;
// Optional: store texture sizes so we can preserve aspect ratio without reloading
static std::map<std::string, std::pair<int,int>> g_bookTextureSizes;
// Font Awesome state (if loaded and merged into ImGui fonts)
static bool g_fontAwesomeLoaded = false;
static ImFont* g_fontAwesome = nullptr;
static bool g_enableDebugLogging = false; // disable expensive per-frame logging unless explicitly needed

// Global badge state enum for status indicators (previously declared inside a local scope
// causing build errors when referenced elsewhere).
enum BadgeState { BS_Available, BS_Unavailable, BS_Borrowed };

// Fine/Penalty System Configuration (Configurable by admin)
static int LOAN_PERIOD_DAYS = 14;  // Standard loan period (2 weeks)
static double FINE_PER_DAY = 5.0;  // Fine amount per day overdue
static double MAX_FINE_AMOUNT = 500.0;  // Maximum fine cap

// Settings popup state
static bool showSettingsPopup = false;
static int tempLoanPeriod = 14;
static float tempFinePerDay = 5.0f;
static float tempMaxFine = 500.0f;
static std::string settingsMessage = "";

static std::string GetExeDirGlobal() {
    char buf[MAX_PATH] = {0};
    if (GetModuleFileNameA(NULL, buf, MAX_PATH) == 0) return std::string();
    std::filesystem::path p(buf);
    return p.parent_path().string();
}

// Write current ImGui window ID stack to a log file (debugging)
static void LogExpanded(const char* tag, const char* name) {
    if (!g_enableDebugLogging) return;
    try {
        ImGuiWindow* w = ImGui::GetCurrentWindow();
        int sz = w ? w->IDStack.Size : -1;
        std::string path = GetExeDirGlobal();
        if (path.empty()) path = ".";
        path += "\\idstack_debug_expanded.log";
        std::ofstream f(path, std::ios::app);
        if (f.is_open()) {
            f << tag << " ";
            if (name) f << name << " "; else f << "<noname> ";
            f << "window=" << (w ? w->Name : std::string("<null>")) << " IDStack=" << sz << "\n";
            f.close();
        }
    } catch(...) {}
}

// Safely call ImGui::EndChild only when inside a child window; write a small debug entry.
static void EndChildSafe() {
    if (!g_enableDebugLogging) {
        ImGui::EndChild();
        return;
    }
    try {
        ImGuiWindow* w = ImGui::GetCurrentWindow();
        std::string path = GetExeDirGlobal();
        if (path.empty()) path = ".";
        path += "\\idstack_debug_expanded.log";
        std::ofstream f(path, std::ios::app);
        if (f.is_open()) {
            f << "SAFE_ENDCHILD window=" << (w ? w->Name : std::string("<null>"))
              << " IDStack=" << (w ? w->IDStack.Size : -1) << "\n";
            f.close();
        }
        ImGui::EndChild();
    } catch(...) {}
}

// General-purpose ID stack logger (writes only when debug logging is enabled)
static void DebugLogIDStack(const std::string& tag) {
    if (!g_enableDebugLogging) return;
    try {
        ImGuiWindow* w = ImGui::GetCurrentWindow();
        int sz = w ? w->IDStack.Size : -1;
        std::string path = GetExeDirGlobal();
        if (path.empty()) path = ".";
        path += "\\idstack_debug.log";
        std::ofstream f(path, std::ios::app);
        if (f.is_open()) {
            f << tag << " window=" << (w ? w->Name : std::string("<null>")) << " IDStack=" << sz << "\n";
            f.close();
        }
    } catch(...) {}
}

// Human-friendly date string for hero headers
static std::string GetFriendlyDateString() {
    std::time_t now = std::time(nullptr);
    std::tm tmNow{};
#if defined(_WIN32)
    localtime_s(&tmNow, &now);
#else
    localtime_r(&now, &tmNow);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmNow, "%A, %B %d");
    return oss.str();
}

// Helper to obtain local time in a cross-platform, warning-free way
static std::tm GetLocalTimeSafe(std::time_t timeValue) {
    std::tm out{};
#if defined(_WIN32)
    if (localtime_s(&out, &timeValue) != 0) {
        std::memset(&out, 0, sizeof(out));
    }
#else
    if (!localtime_r(&timeValue, &out)) {
        std::memset(&out, 0, sizeof(out));
    }
#endif
    return out;
}

// Get current date/time as string for logging
static std::string GetCurrentDateTimeString() {
    std::time_t now = std::time(nullptr);
    std::tm tmNow = GetLocalTimeSafe(now);
    std::ostringstream oss;
    oss << std::put_time(&tmNow, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Reusable stat card used across dashboards to present key metrics
static void RenderStatCard(const char* id, const char* label, const std::string& value, const ImVec4& accent, float width, const char* detail = nullptr) {
    ImVec4 bgColor = ImVec4(accent.x, accent.y, accent.z, 0.22f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
        if (ImGui::BeginChild(id, ImVec2(width, 100), true, ImGuiWindowFlags_NoScrollbar)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.86f, 0.92f, 0.95f));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        auto brighten = [](float component) {
            component += 0.25f;
            if (component > 1.0f) component = 1.0f;
            return component;
        };
        ImVec4 accentBright = ImVec4(brighten(accent.x), brighten(accent.y), brighten(accent.z), 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, accentBright);
        ImGui::SetWindowFontScale(1.15f);
        ImGui::Text("%s", value.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        if (detail && detail[0] != '\0') {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.78f, 0.84f, 0.95f));
            ImGui::TextWrapped("%s", detail);
            ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

// Styled child helpers for sidebar sections
static bool BeginSidebarCard(const char* id, float height, const ImVec4& bgColor, ImVec2 padding = ImVec2(16, 14)) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
        return ImGui::BeginChild(id, ImVec2(0, height > 0.0f ? height : 0.0f), true, ImGuiWindowFlags_NoScrollbar);
}

static void EndSidebarCard() {
    ImGui::EndChild();
        ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

// Unified sidebar palette so student/admin columns share identical styling.
static const ImVec4 SIDEBAR_PROFILE_BG  = ImVec4(0.12f, 0.15f, 0.19f, 0.95f);
static const ImVec4 SIDEBAR_ACTION_BG   = ImVec4(0.16f, 0.20f, 0.25f, 0.95f);
static const ImVec4 SIDEBAR_CATEGORY_BG = ImVec4(0.11f, 0.13f, 0.16f, 0.95f);
static const ImVec4 SIDEBAR_WATCH_BG    = ImVec4(0.13f, 0.16f, 0.21f, 0.95f);
static const ImVec4 SIDEBAR_BORROW_BG   = ImVec4(0.13f, 0.17f, 0.22f, 0.95f);
static const ImVec4 SIDEBAR_GLOBAL_BG   = ImVec4(0.12f, 0.16f, 0.21f, 0.95f);

// Forward declarations for branding helpers referenced before their full definitions
static bool ResolveBgLogoTexture(ID3D11ShaderResourceView** outSrv, ImVec2* intrinsicSize);
static bool DrawSidebarLogoImage(float maxHeight);

static void RenderSidebarLogoHeader(const char* title, const char* subtitle, bool includeDivider = true) {
    ImGui::PushID(title);
    ID3D11ShaderResourceView* logoSrv = nullptr;
    ImVec2 nativeSize(0.0f, 0.0f);
    bool haveLogo = ResolveBgLogoTexture(&logoSrv, &nativeSize);
    float maxLogoHeight = 56.0f;

    ImGui::BeginGroup();
    if (haveLogo && nativeSize.x > 0.0f && nativeSize.y > 0.0f) {
        float scale = maxLogoHeight / nativeSize.y;
        if (scale > 1.0f) scale = 1.0f;
        float drawW = nativeSize.x * scale;
        float drawH = nativeSize.y * scale;
        ImGui::Image((ImTextureID)logoSrv, ImVec2(drawW, drawH));
        ImGui::SameLine(0.0f, 14.0f);
    }
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.93f, 1.0f, 1.0f));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    if (subtitle && subtitle[0] != '\0') {
        ImGui::TextDisabled("%s", subtitle);
    }
    ImGui::EndGroup();
    ImGui::EndGroup();
    ImGui::PopID();
    if (includeDivider) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }
}

// Load image from file into a D3D11 shader resource view. Returns true on success.
bool LoadTextureFromFile(const char* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height) {
    if (!filename || !out_srv) return false;
    *out_srv = nullptr;
    // Try several candidate paths so icon paths resolve regardless of working directory.
    auto GetExeDir = []() -> std::string {
        char buf[MAX_PATH] = {0};
        if (GetModuleFileNameA(NULL, buf, MAX_PATH) == 0) return std::string();
        std::filesystem::path p(buf);
        return p.parent_path().string();
    };

    std::vector<std::string> candidates;
    std::string orig(filename);
    candidates.push_back(orig);

    std::string exeDir = GetExeDir();
    if (!exeDir.empty()) {
        // exeDir + \ + filename
        std::string combo = exeDir + "\\" + orig;
        candidates.push_back(combo);

        // exe parent (one level up) — common when exe lives in Release/ and icons/ is next to it
        std::filesystem::path exeP(exeDir);
        std::string exeParent;
        if (exeP.has_parent_path()) exeParent = exeP.parent_path().string();
        if (!exeParent.empty()) {
            std::string parentCombo = exeParent + "\\" + orig;
            candidates.push_back(parentCombo);
        }

        // If filename is just a basename or starts with icons/, also try exeDir\icons\basename
        std::filesystem::path p(orig);
        std::string base = p.filename().string();
        std::string iconsCombo = exeDir + "\\icons\\" + base;
        candidates.push_back(iconsCombo);

        if (!exeParent.empty()) {
            std::string parentIcons = exeParent + "\\icons\\" + base;
            candidates.push_back(parentIcons);
        }
    }

    // Normalize separators and dedupe
    for (auto &s : candidates) {
        std::replace(s.begin(), s.end(), '/', '\\');
    }
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    int x = 0, y = 0, n = 0;
    unsigned char* data = nullptr;
    std::string usedPath;

    // Optional debug log for cover loading attempts (appends to exe dir when available)
    std::ofstream dbgLog;
    if (!exeDir.empty()) dbgLog.open(exeDir + "\\cover_load.log", std::ios::app);
    else dbgLog.open("cover_load.log", std::ios::app);

    for (const auto &cand : candidates) {
        bool exists = std::filesystem::exists(cand);
        if (dbgLog.is_open()) dbgLog << "Trying: " << cand << " exists=" << (exists?"1":"0") << std::endl;
        if (exists) {
            data = stbi_load(cand.c_str(), &x, &y, &n, 4);
            if (data) { usedPath = cand; if (dbgLog.is_open()) dbgLog << "Loaded (exists): " << cand << " size=" << x << "x" << y << std::endl; break; }
        } else {
            // try to load even if filesystem says not exists (handles UNC / permissions)
            data = stbi_load(cand.c_str(), &x, &y, &n, 4);
            if (data) { usedPath = cand; if (dbgLog.is_open()) dbgLog << "Loaded (no-exists): " << cand << " size=" << x << "x" << y << std::endl; break; }
            else { if (dbgLog.is_open()) dbgLog << "stbi_load failed for: " << cand << std::endl; }
        }
    }
    if (dbgLog.is_open()) dbgLog.flush();

    if (!data) return false;

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = x;
    desc.Height = y;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData;
    initData.pSysMem = data;
    initData.SysMemPitch = x * 4;
    initData.SysMemSlicePitch = 0;

    ID3D11Texture2D* pTex = nullptr;
    HRESULT hr = g_pd3dDevice->CreateTexture2D(&desc, &initData, &pTex);
    stbi_image_free(data);
    if (FAILED(hr) || !pTex) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    hr = g_pd3dDevice->CreateShaderResourceView(pTex, &srvDesc, out_srv);
    pTex->Release();
    if (FAILED(hr) || !*out_srv) return false;

    if (out_width) *out_width = x;
    if (out_height) *out_height = y;
    return true;
}

void CleanupBookTextures() {
    for (auto &p : g_bookTextures) {
        if (p.second) p.second->Release();
    }
    g_bookTextures.clear();
}

struct DBStatus { bool isConnected = false; } g_db;

// ---------------- CSV Paths ----------------
// These will be resolved at runtime relative to the executable directory
// so the app does not depend on the current working directory.
std::string basePath;
std::string usersCSV;
std::string userProfileCSV;
std::string booksCSV;
std::string historyCSV;
std::string favoritesCSV;

// Branding helpers reuse the cached bglogo texture without duplicating load logic.
static bool ResolveBgLogoTexture(ID3D11ShaderResourceView** outSrv, ImVec2* intrinsicSize = nullptr) {
    if (!outSrv) return false;
    *outSrv = nullptr;

    int loadedW = 0;
    int loadedH = 0;

    auto texIt = g_bookTextures.find("bglogo");
    if (texIt != g_bookTextures.end()) {
        *outSrv = texIt->second;
    }

    if (!*outSrv) {
        std::vector<std::string> logoCandidates;
        if (!basePath.empty()) logoCandidates.push_back(basePath + "bglogo\\bglogo.png");
        logoCandidates.push_back(".\\db\\bglogo\\bglogo.png");
        std::string exeDir = GetExeDirGlobal();
        if (!exeDir.empty()) logoCandidates.push_back(exeDir + "\\db\\bglogo\\bglogo.png");

        for (const auto& candidate : logoCandidates) {
            if (candidate.empty()) continue;
            if (LoadTextureFromFile(candidate.c_str(), outSrv, &loadedW, &loadedH)) {
                g_bookTextures["bglogo"] = *outSrv;
                if (loadedW > 0 && loadedH > 0) {
                    g_bookTextureSizes["bglogo"] = { loadedW, loadedH };
                }
                break;
            }
        }
    }

    if (!*outSrv) return false;

    auto sizeIt = g_bookTextureSizes.find("bglogo");
    if (sizeIt != g_bookTextureSizes.end()) {
        loadedW = sizeIt->second.first;
        loadedH = sizeIt->second.second;
    }
    if (loadedW == 0 || loadedH == 0) {
        loadedW = 1024;
        loadedH = 512;
    }

    if (intrinsicSize) {
        intrinsicSize->x = static_cast<float>(loadedW);
        intrinsicSize->y = static_cast<float>(loadedH);
    }
    return true;
}

static bool DrawSidebarLogoImage(float maxHeight) {
    ID3D11ShaderResourceView* logoSrv = nullptr;
    ImVec2 nativeSize(0.0f, 0.0f);
    if (!ResolveBgLogoTexture(&logoSrv, &nativeSize)) {
        return false;
    }
    if (nativeSize.x <= 0.0f || nativeSize.y <= 0.0f) {
        return false;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x <= 0.0f) return false;
    float heightLimit = maxHeight > 0.0f ? maxHeight : nativeSize.y;
    if (avail.y > 0.0f) {
        heightLimit = (std::min)(heightLimit, avail.y);
    }

    float scaleX = avail.x / nativeSize.x;
    float scaleY = heightLimit / nativeSize.y;
    float scale = (std::min)(scaleX, scaleY);
    if (scale > 1.0f) scale = 1.0f;
    float drawW = nativeSize.x * scale;
    float drawH = nativeSize.y * scale;
    if (drawW <= 0.0f || drawH <= 0.0f) return false;

    float baseX = ImGui::GetCursorPosX();
    float offsetX = (avail.x - drawW) * 0.5f;
    if (offsetX > 0.0f) {
        ImGui::SetCursorPosX(baseX + offsetX);
    }

    // Align to whole pixels to prevent the texture from looking blurry when downscaled.
    ImVec2 cursorScreen = ImGui::GetCursorScreenPos();
    ImVec2 snapped(cursorScreen.x, cursorScreen.y);
    snapped.x = ImFloor(snapped.x);
    snapped.y = ImFloor(snapped.y);
    ImGui::SetCursorScreenPos(snapped);

    ImGui::Image((ImTextureID)logoSrv, ImVec2(drawW, drawH));

    if (offsetX > 0.0f) {
        ImGui::SetCursorPosX(baseX);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
    return true;
}

// ---------------- Forward declarations ----------------
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Utility functions
std::string trim(const std::string& s);
void EnsureCSVExists();
bool ConnectToDatabase();
void SaveFineSettings();
void LoadFineSettings();
UserProfile VerifyUserCredentials(const char* username_char, const char* password_char);
// Book/CSV ingestion + caching helpers
void LoadBooksFromCSV();
bool LoadBooksFromCSVPath(const std::string& path, std::vector<Book>& outBooks);
void LoadBorrowHistory();
bool IsBookBorrowed(const std::string& title);
std::string GenerateReturnCode();
bool SaveBorrowEntry(const std::string& bookTitle, const std::string& user); // changed to bool
void SaveHistoryToCSV();
// User roster + profile persistence
void LoadUsersFromCSV();
void LoadUserProfilesFromCSV();
bool SaveUsersToCSV();
void SaveUserProfilesToCSV();
void LogActivity(const std::string& username, const std::string& action, const std::string& details = "");
void LoadActivityLogsFromCSV();
void SaveActivityLogsToCSV();
void ExportActivityLogsToPDF(const std::string& filename, const std::vector<ActivityLog>& logs);
void ReturnBook(const std::string& bookTitle, const std::string& username, const std::string& returnCode);
std::string CalculateTimeElapsed(const std::string& borrowDate);
int DaysSinceBorrow(const std::string& borrowDate);
std::string CalculateDueDate(const std::string& borrowDate);
int DaysUntilDue(const std::string& dueDate);
double CalculateFine(const std::string& dueDate, const std::string& returnDate = "");
double GetTotalUnpaidFines(const std::string& username);
void PruneFavoritesToExistingBooks();
void LoadFavoritesFromCSV();
void SaveFavoritesToCSV();
bool IsBookFavorited(const std::string& username, const std::string& title);
void ToggleFavorite(const std::string& username, const std::string& title);
std::vector<std::string> GetFavoritesForUser(const std::string& username);
int GetFavoriteCountForBook(const std::string& title);
BookDiffResult ComputeBookDiff(const std::vector<Book>& currentList, const std::vector<Book>& incomingList);
const char* BookDiffTypeLabel(BookDiffType type);
ImVec4 BookDiffTypeColor(BookDiffType type);

// UI Rendering Functions
void RenderLoginScreen(float fullSizeX, float fullSizeY, bool& loggedIn, char* username, char* password);
void RenderProfilePopup(float fullSizeX, float fullSizeY, UserProfile& user);
void RenderSettingsPopup(float fullSizeX, float fullSizeY);
void RenderBookDetailsPopup(float fullSizeX, float fullSizeY, const char* currentUsername);
void RenderStudentDashboard(float fullSizeX, float fullSizeY, bool& loggedIn, const char* currentUsername);
void RenderAdminDashboard(float fullSizeX, float fullSizeY, bool& loggedIn, const char* currentUsername);

// ---------------- Utility Functions Implementation ----------------
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

void EnsureCSVExists() {
    if (!std::filesystem::exists(basePath)) {
        std::cout << "Creating directory: " << basePath << std::endl;
        std::filesystem::create_directories(basePath);
    }

    std::vector<std::pair<std::string, std::string>> files = {
        { usersCSV,
          "admin,admin1,admin\n"
          "ctsolomon,pass1,student\n"
          "godhelpme,pass2,student\n" },
        { userProfileCSV,
          "admin,System Administrator,IT Dept,N/A,999\n"
          "ctsolomon,Charlie T. Solomon,BS-CS,Male,20\n"
          "godhelpme,N/A,N/A,N/A,0\n"},
                { booksCSV,
                    "The Great Gatsby,F. Scott Fitzgerald,Fiction,icons/book1.png\n"
                    "Introduction to C++,Bjarne Stroustrup,Computer Science,icons/book2.png\n"
                    "Python Basics,Guido van Rossum,Computer Science,icons/book3.png\n"
                    "The Lord of the Rings,J.R.R. Tolkien,Fantasy,icons/book4.png\n"
                    "Introduction to Algorithms,Thomas H. Cormen;Charles E. Leiserson;Ronald L. Rivest;Clifford Stein,Computer Science,icons/book5.png\n"
                    "The C++ Programming Language,Bjarne Stroustrup,Computer Science,icons/book6.png\n"
                    "Structure and Interpretation of Computer Programs,Harold Abelson;Gerald Jay Sussman,Computer Science,icons/book7.png\n"
                    "Design Patterns,Erich Gamma;Richard Helm;Ralph Johnson;John Vlissides,Computer Science,icons/book8.png\n"
                    "Clean Code,Robert C. Martin,Computer Science,icons/book9.png\n"
                    "Operating System Concepts,Abraham Silberschatz;Peter Baer Galvin;Greg Gagne,Computer Science,icons/book10.png\n"
                    "Computer Networks,Andrew S. Tanenbaum,Computer Science,icons/book11.png\n"
                    "Artificial Intelligence: A Modern Approach,Stuart Russell;Peter Norvig,Computer Science,icons/book12.png\n"
                    "Compilers: Principles, Techniques, and Tools,Alfred V. Aho;Monica S. Lam;Ravi Sethi;Jeffrey D. Ullman,Computer Science,icons/book13.png\n"
                    "Deep Learning,Ian Goodfellow;Yoshua Bengio;Aaron Courville,Computer Science,icons/book14.png\n"
                    "Modern Operating Systems,Andrew S. Tanenbaum,Computer Science,icons/book15.png\n"
                    "Computer Organization and Design,David A. Patterson;John L. Hennessy,Computer Science,icons/book16.png\n"
                    "The Art of Computer Programming,Donald E. Knuth,Computer Science,icons/book17.png\n"
                    "Introduction to the Theory of Computation,Michael Sipser,Computer Science,icons/book18.png\n"
                    "Database System Concepts,Abraham Silberschatz;Henry F. Korth;S. Sudarshan,Computer Science,icons/book19.png\n"
                    "Programming Pearls,Jon Bentley,Computer Science,icons/book20.png\n"
                    "Algorithms in C++,Robert Sedgewick,Computer Science,icons/book21.png\n"
                    "Machine Learning,Tom M. Mitchell,Computer Science,icons/book22.png\n"
                    "Numerical Recipes,William H. Press;et al.,Computer Science,icons/book23.png\n"},
        { historyCSV, "" },
        { favoritesCSV, "" }
    };

    for (auto& f : files) {
        if (!std::filesystem::exists(f.first)) {
            std::cout << "Creating file: " << f.first << std::endl;
            std::ofstream file(f.first);
            file << f.second;
        }
    }
}

bool ConnectToDatabase() {
    g_db.isConnected = std::filesystem::exists(usersCSV) &&
        std::filesystem::exists(userProfileCSV) &&
        std::filesystem::exists(booksCSV) &&
        std::filesystem::exists(historyCSV) &&
        std::filesystem::exists(favoritesCSV);
    return g_db.isConnected;
}

// Generate a unique 6-digit numeric return code (ensures uniqueness across existing borrowHistory)
std::string GenerateReturnCode() {
    auto codeExists = [](const std::string& c) {
        for (const auto& h : borrowHistory) {
            if (!h.returnCode.empty() && h.returnCode == c) return true;
        }
        return false;
        };

    std::string code;
    int attempts = 0;
    do {
        code.clear();
        for (int i = 0; i < 6; ++i) {
            code += std::to_string(rand() % 10);
        }
        attempts++;
        if (attempts > 1000) break; // fail-safe
    } while (codeExists(code));

    return code;
}

void SaveHistoryToCSV() {
    std::ofstream file(historyCSV);
    if (!file.is_open()) return;

    for (const auto& h : borrowHistory) {
        file << h.bookTitle << ","
            << h.borrowerName << ","
            << h.borrowDate << ","
            << h.dueDate << ","
            << (h.isReturned ? "1" : "0") << ","
            << h.returnDate << ","
            << h.returnCode << ","
            << h.fine << ","
            << (h.finePaid ? "1" : "0") << "\n";
    }
}

// Save the current borrowHistory into a simple multipage PDF.
// Produces a basic PDF using standard Type1 Helvetica font. Each entry gets its own page.
// This is a minimal PDF writer (no external deps) suitable for small printable lists.
static bool SaveReturnCodesPDF(const std::string& outPath) {
    try {
        auto escape = [](const std::string& s) {
            std::string o; o.reserve(s.size());
            for (char c : s) {
                if (c == '(') o += "\\(";
                else if (c == ')') o += "\\)";
                else if (c == '\\') o += "\\\\";
                else o += c;
            }
            return o;
        };

        std::vector<std::string> objs;

        // object 1: Catalog (will reference pages object 2)
        objs.push_back("<< /Type /Catalog /Pages 2 0 R >>\n");

        // Placeholder for pages object (object 2). we'll fill kids later
        objs.push_back(std::string()); // 2

        // Font object (object 10)
        size_t fontObjIndex = 9; // zero-based index -> object number = index+1
        while (objs.size() <= fontObjIndex) objs.push_back(std::string());
        objs[fontObjIndex] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n";

        std::vector<int> pageObjNums; // store object numbers for pages
        std::vector<std::string> contentObjs; // content stream objects (will be appended)

        // Create one page per borrowHistory entry. If no entries, create a single page with header.
        if (borrowHistory.empty()) {
            // Create one page with message
            std::string content;
            content += "BT\n/F1 18 Tf\n72 720 Td\n(" + escape("No borrow history entries.") + ") Tj\nET\n";
            // push content obj
            contentObjs.push_back(content);
        }
        else {
            for (const auto& h : borrowHistory) {
                std::string content;
                content += "BT\n/F1 18 Tf\n72 720 Td\n(" + escape(std::string("Title: ") + h.bookTitle) + ") Tj\n";
                content += "0 -26 Td\n/F1 14 Tf\n(" + escape(std::string("Borrower: ") + h.borrowerName) + ") Tj\n";
                content += "0 -20 Td\n(" + escape(std::string("Return Code: ") + h.returnCode) + ") Tj\n";
                content += "0 -20 Td\n(" + escape(std::string("Status: ") + (h.isReturned ? std::string("Returned") : std::string("Borrowed"))) + ") Tj\nET\n";
                contentObjs.push_back(content);
            }
        }

        // Append page objects and their content stream objects
        for (size_t i = 0; i < contentObjs.size(); ++i) {
            // page object (will reference content object number which is next)
            // We'll push a placeholder now; actual content object will be appended later
            objs.push_back(std::string()); // page object placeholder
            pageObjNums.push_back((int)objs.size()); // object number (1-based)
            // content obj appended after pages will be pushed to objs as well
            objs.push_back(std::string()); // content stream placeholder
        }

        // Fill pages object (object 2)
        {
            std::ostringstream ss;
            ss << "<< /Type /Pages /Kids [ ";
            for (int pnum : pageObjNums) ss << pnum << " 0 R ";
            ss << "] /Count " << pageObjNums.size() << " >>\n";
            objs[1] = ss.str();
        }

        // Now fill page objects and content objects accordingly. Note: font is object 10.
        int contentIndex = 0;
        for (size_t i = 0; i < pageObjNums.size(); ++i) {
            int pageObjNumber = pageObjNums[i];
            int contentObjNumber = pageObjNumber + 1; // we allocated page then content sequentially
            std::ostringstream pageSS;
            pageSS << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 " << (fontObjIndex+1) << " 0 R >> >> /Contents " << contentObjNumber << " 0 R >>\n";
            objs[pageObjNumber - 1] = pageSS.str();

            // Content stream: we need Length and stream wrappers handled at write time, so store raw payload here
            std::string payload = contentObjs[contentIndex++];
            // We'll convert payload into a stream object when serializing (with /Length n)
            objs[contentObjNumber - 1] = payload; // mark content placeholder
        }

        // Build the final PDF by computing byte offsets for each object and writing xref
        std::ostringstream out;
        out << "%PDF-1.4\n%\xFF\xFF\xFF\xFF\n";

        std::vector<long long> offsets;
        for (size_t i = 0; i < objs.size(); ++i) {
            offsets.push_back((long long)out.tellp());
            int objnum = (int)i + 1;
            out << objnum << " 0 obj\n";

            // If this is a content stream object (we identified them by payload containing 'BT' etc.) then write as stream with Length
            std::string &body = objs[i];
            bool isContent = false;
            if (!body.empty()) {
                // heuristics: content streams contain 'BT' operator
                if (body.find("BT") != std::string::npos) isContent = true;
            }

            if (isContent) {
                std::string streamData = body;
                std::ostringstream sb;
                sb << streamData;
                std::string sdat = sb.str();
                out << "<< /Length " << sdat.size() << " >>\nstream\n";
                out << sdat;
                out << "endstream\n";
            }
            else {
                out << body;
            }

            out << "endobj\n";
        }

        long long xrefPos = (long long)out.tellp();
        out << "xref\n0 " << (objs.size() + 1) << "\n";
        out << "0000000000 65535 f \n";
        for (size_t i = 0; i < offsets.size(); ++i) {
            out << std::setw(10) << std::setfill('0') << offsets[i] << " 00000 n \n";
        }

        out << "trailer\n<< /Size " << (objs.size() + 1) << " /Root 1 0 R >>\nstartxref\n" << xrefPos << "\n%%EOF\n";

        // Write to file (binary)
        std::ofstream fout(outPath, std::ios::binary);
        if (!fout.is_open()) return false;
        std::string final = out.str();
        fout.write(final.c_str(), (std::streamsize)final.size());
        fout.close();
        return true;
    }
    catch (...) {
        return false;
    }
}

// Save a single HistoryEntry as a one-page printable PDF (big return code + details).
// Show a native Save File dialog and put the chosen file path into outPath (returns true on success)
static bool PickSaveFilePath(std::string& outPath, const std::string& defaultName = "return_code.pdf") {
    char filename[MAX_PATH] = {0};
    strncpy_s(filename, sizeof(filename), defaultName.c_str(), _TRUNCATE);

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "PDF Files\0*.pdf\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn)) {
        outPath = std::string(filename);
        return true;
    }
    return false;
}

// Save a single HistoryEntry as a one-page printable PDF (centered large return code, margins)
static bool SaveSingleReturnCodePDF(const HistoryEntry& h, const std::string& outPath) {
    try {
        auto escape = [](const std::string& s) {
            std::string o; o.reserve(s.size());
            for (char c : s) {
                if (c == '(') o += "\\(";
                else if (c == ')') o += "\\)";
                else if (c == '\\') o += "\\\\";
                else o += c;
            }
            return o;
        };

        // Code128 patterns for symbols 0..106 (six module widths, bars/spaces alternating)
        static const char* CODE128_PATTERNS[107] = {
            "212222","222122","222221","121223","121322","131222","122213","122312","132212","221213",
            "221312","231212","112232","122132","122231","113222","123122","123221","223211","221132",
            "221231","213212","223112","312131","311222","321122","321221","312212","322112","322211",
            "212123","212321","232121","111323","131123","131321","112313","132113","132311","211313",
            "231113","231311","112133","112331","132131","113123","113321","133121","313121","211331",
            "231131","213113","213311","213131","311123","311321","331121","312113","312311","332111",
            "314111","221411","431111","111224","111422","121124","121421","141122","141221","112214",
            "112412","122114","122411","142112","142211","241211","221114","413111","241112","134111",
            "111242","121142","121241","114212","124112","124211","411212","421112","421211","212141",
            "214121","412121","111143","111341","131141","114113","114311","411113","411311","113141",
            "114131","311141","411131","211412","211214","211232","2331112"
        };

        // Prepare numeric data for Code128-C (pairs of digits). If odd length, prepend 0.
        std::string digits;
        for (char c : h.returnCode) if (std::isdigit((unsigned char)c)) digits.push_back(c);
        if (digits.empty()) digits = "00";
        if (digits.size() % 2 == 1) digits = std::string("0") + digits;

        std::vector<int> codes;
        // Start Code C = 105
        const int startCode = 105;
        codes.push_back(startCode);

        for (size_t i = 0; i < digits.size(); i += 2) {
            int val = (digits[i]-'0')*10 + (digits[i+1]-'0');
            codes.push_back(val);
        }

        // Checksum: (start) + sum(code_i * position)
        int checksum = startCode;
        for (size_t i = 1; i < codes.size(); ++i) {
            checksum += codes[i] * (int)i;
        }
        checksum = checksum % 103;
        codes.push_back(checksum);
        // Stop code = 106
        codes.push_back(106);

        // Build barcode pattern widths sequence
        std::vector<int> modules;
        for (int v : codes) {
            if (v < 0 || v > 106) continue;
            const char* p = CODE128_PATTERNS[v];
            for (int k = 0; k < 6; ++k) modules.push_back(p[k]-'0');
        }

        // PDF page/layout
        const int pageW = 612;
        const int pageH = 792;
        const int margin = 72;

        // barcode sizing
        float moduleWidth = 1.8f; // pts per module (tweak for scanner reliability)
        float barcodeWidth = 0.0f;
        for (int m : modules) barcodeWidth += m * moduleWidth;
        float barcodeHeight = 48.0f;

        float barcodeX = (pageW - barcodeWidth) * 0.5f;
        float barcodeY = pageH * 0.35f; // place barcode below center

        std::string title = std::string("Title: ") + h.bookTitle;
        std::string borrower = std::string("Borrower: ") + h.borrowerName;
        std::string codeLabel = std::string("Return Code: ") + h.returnCode;

        // Construct PDF objects: Catalog, Pages, Page, Font, Content
        std::vector<std::string> objs(5);
        objs[0] = "<< /Type /Catalog /Pages 2 0 R >>\n";
        objs[1] = "<< /Type /Pages /Kids [ 3 0 R ] /Count 1 >>\n";
        std::ostringstream pageSS;
        pageSS << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " << pageW << " " << pageH << "] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>\n";
        objs[2] = pageSS.str();
        objs[3] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n";

        // Build content stream: draw barcode rectangles, then text
        std::ostringstream content;
        // Draw bars: iterate modules, drawing bars for every even index (start with bar)
        float x = barcodeX;
        for (size_t mi = 0; mi < modules.size(); ++mi) {
            float w = modules[mi] * moduleWidth;
            if (mi % 2 == 0) {
                // draw filled rect at (x, barcodeY) with width w and barcodeHeight
                content << std::fixed << std::setprecision(2) << x << " " << barcodeY << " " << w << " " << barcodeHeight << " re f\n";
            }
            x += w;
        }

        // Add text above barcode (Title) and below (Return Code)
        float titleFS = 14;
        float borrowerFS = 12;
        float codeFS = 24;

        float titleY = barcodeY + barcodeHeight + 36.0f;
        float borrowerY = barcodeY + barcodeHeight + 18.0f;
        float codeY = barcodeY - 36.0f;

        content << "BT\n/F1 " << (int)titleFS << " Tf\n" << margin << " " << titleY << " Td\n(" << escape(title) << ") Tj\nET\n";
        content << "BT\n/F1 " << (int)borrowerFS << " Tf\n" << margin << " " << borrowerY << " Td\n(" << escape(borrower) << ") Tj\nET\n";
        content << "BT\n/F1 " << (int)codeFS << " Tf\n" << margin << " " << codeY << " Td\n(" << escape(codeLabel) << ") Tj\nET\n";

        objs[4] = content.str();

        // Serialize PDF
        std::ostringstream out;
        out << "%PDF-1.4\n%\xFF\xFF\xFF\xFF\n";
        std::vector<long long> offsets;
        for (size_t i = 0; i < objs.size(); ++i) {
            offsets.push_back((long long)out.tellp());
            int objnum = (int)i + 1;
            out << objnum << " 0 obj\n";
            if (i == 4) {
                std::string sdat = objs[i];
                out << "<< /Length " << sdat.size() << " >>\nstream\n";
                out << sdat;
                out << "endstream\n";
            } else {
                out << objs[i];
            }
            out << "endobj\n";
        }
        long long xrefPos = (long long)out.tellp();
        out << "xref\n0 " << (objs.size() + 1) << "\n";
        out << "0000000000 65535 f \n";
        for (size_t i = 0; i < offsets.size(); ++i) {
            out << std::setw(10) << std::setfill('0') << offsets[i] << " 00000 n \n";
        }
        out << "trailer\n<< /Size " << (objs.size() + 1) << " /Root 1 0 R >>\nstartxref\n" << xrefPos << "\n%%EOF\n";

        std::ofstream fout(outPath, std::ios::binary);
        if (!fout.is_open()) return false;
        std::string final = out.str();
        fout.write(final.c_str(), (std::streamsize)final.size());
        fout.close();
        return true;
    }
    catch (...) { return false; }
}

// Loads borrow history into memory
void LoadBorrowHistory() {
    borrowHistory.clear();
    std::ifstream file(historyCSV);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        HistoryEntry h;
        std::string isReturnedStr, fineStr, finePaidStr;

        // Try to load with new format (with dueDate, fine, finePaid)
        if (std::getline(ss, h.bookTitle, ',') &&
            std::getline(ss, h.borrowerName, ',') &&
            std::getline(ss, h.borrowDate, ',') &&
            std::getline(ss, h.dueDate, ',') &&
            std::getline(ss, isReturnedStr, ',') &&
            std::getline(ss, h.returnDate, ',') &&
            std::getline(ss, h.returnCode, ',') &&
            std::getline(ss, fineStr, ',') &&
            std::getline(ss, finePaidStr)) {

            h.bookTitle = trim(h.bookTitle);
            h.borrowerName = trim(h.borrowerName);
            h.borrowDate = trim(h.borrowDate);
            h.dueDate = trim(h.dueDate);
            isReturnedStr = trim(isReturnedStr);
            h.returnDate = trim(h.returnDate);
            h.returnCode = trim(h.returnCode);
            fineStr = trim(fineStr);
            finePaidStr = trim(finePaidStr);

            h.isReturned = (isReturnedStr == "1");
            h.finePaid = (finePaidStr == "1");
            
            try {
                h.fine = std::stod(fineStr);
            } catch (...) {
                h.fine = 0.0;
            }
            
            // If dueDate is empty (old format), calculate it from borrowDate
            if (h.dueDate.empty() && !h.borrowDate.empty()) {
                h.dueDate = CalculateDueDate(h.borrowDate);
            }
            
            // Recalculate fine if book is returned or currently overdue
            if (!h.dueDate.empty()) {
                if (h.isReturned && !h.returnDate.empty()) {
                    h.fine = CalculateFine(h.dueDate, h.returnDate);
                } else if (!h.isReturned) {
                    h.fine = CalculateFine(h.dueDate);
                }
            }

            borrowHistory.push_back(h);
        }
        else {
            // Try old format (without dueDate, fine, finePaid) for backward compatibility
            ss.clear();
            ss.str(line);
            
            if (std::getline(ss, h.bookTitle, ',') &&
                std::getline(ss, h.borrowerName, ',') &&
                std::getline(ss, h.borrowDate, ',') &&
                std::getline(ss, isReturnedStr, ',') &&
                std::getline(ss, h.returnDate, ',') &&
                std::getline(ss, h.returnCode)) {

                h.bookTitle = trim(h.bookTitle);
                h.borrowerName = trim(h.borrowerName);
                h.borrowDate = trim(h.borrowDate);
                isReturnedStr = trim(isReturnedStr);
                h.returnDate = trim(h.returnDate);
                h.returnCode = trim(h.returnCode);

                h.isReturned = (isReturnedStr == "1");
                
                // Calculate dueDate from borrowDate
                h.dueDate = CalculateDueDate(h.borrowDate);
                
                // Calculate fine
                if (h.isReturned && !h.returnDate.empty()) {
                    h.fine = CalculateFine(h.dueDate, h.returnDate);
                } else if (!h.isReturned) {
                    h.fine = CalculateFine(h.dueDate);
                }
                
                h.finePaid = false;

                borrowHistory.push_back(h);
            }
        }
    }
}

std::string CalculateTimeElapsed(const std::string& borrowDate) {
    std::tm tm = {};
    std::stringstream ss(borrowDate);

    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) return "Error calculating time.";

    std::time_t borrowTime = std::mktime(&tm);
    std::time_t currentTime = std::time(nullptr);

    double seconds = std::difftime(currentTime, borrowTime);

    long long minutes = static_cast<long long>(seconds / 60.0);
    long long hours = minutes / 60;
    long long days = hours / 24;

    if (days > 0) {
        return std::to_string(days) + "d " + std::to_string(hours % 24) + "h " + std::to_string(minutes % 60) + "m";
    }
    else if (hours > 0) {
        return std::to_string(hours) + "h " + std::to_string(minutes % 60) + "m";
    }
    else if (minutes > 0) {
        return std::to_string(minutes) + "m";
    }
    else {
        return "Just borrowed.";
    }
}

int DaysSinceBorrow(const std::string& borrowDate) {
    std::tm tm = {};
    std::stringstream ss(borrowDate);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) return 0;
    std::time_t borrowTime = std::mktime(&tm);
    std::time_t currentTime = std::time(nullptr);
    double seconds = std::difftime(currentTime, borrowTime);
    if (seconds < 0) seconds = 0;
    return (int)(seconds / (60.0 * 60.0 * 24.0));
}

// Calculate due date from borrow date (adds LOAN_PERIOD_DAYS)
std::string CalculateDueDate(const std::string& borrowDate) {
    std::tm tm = {};
    std::stringstream ss(borrowDate);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) return borrowDate;
    
    std::time_t borrowTime = std::mktime(&tm);
    std::time_t dueTime = borrowTime + (LOAN_PERIOD_DAYS * 24 * 60 * 60);
    std::tm dueTm = GetLocalTimeSafe(dueTime);
    
    char dateStr[30];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", &dueTm);
    return std::string(dateStr);
}

// Calculate days until due date (negative means overdue)
int DaysUntilDue(const std::string& dueDate) {
    if (dueDate.empty()) return 0;
    std::tm tm = {};
    std::stringstream ss(dueDate);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) return 0;
    
    std::time_t dueTime = std::mktime(&tm);
    std::time_t currentTime = std::time(nullptr);
    double seconds = std::difftime(dueTime, currentTime);
    return (int)(seconds / (60.0 * 60.0 * 24.0));
}

// Calculate fine for an overdue book
double CalculateFine(const std::string& dueDate, const std::string& returnDate) {
    if (dueDate.empty()) return 0.0;
    
    std::tm dueTm = {};
    std::stringstream dueSs(dueDate);
    dueSs >> std::get_time(&dueTm, "%Y-%m-%d %H:%M:%S");
    if (dueSs.fail()) return 0.0;
    std::time_t dueTime = std::mktime(&dueTm);
    
    std::time_t checkTime;
    if (!returnDate.empty()) {
        // Calculate fine based on return date
        std::tm returnTm = {};
        std::stringstream returnSs(returnDate);
        returnSs >> std::get_time(&returnTm, "%Y-%m-%d %H:%M:%S");
        if (returnSs.fail()) return 0.0;
        checkTime = std::mktime(&returnTm);
    } else {
        // Calculate fine based on current time (for unreturned books)
        checkTime = std::time(nullptr);
    }
    
    double seconds = std::difftime(checkTime, dueTime);
    if (seconds <= 0) return 0.0;  // Not overdue
    
    int overdueDays = (int)(seconds / (60.0 * 60.0 * 24.0)) + 1;  // Round up
    double fine = overdueDays * FINE_PER_DAY;
    
    // Apply maximum fine cap
    if (fine > MAX_FINE_AMOUNT) fine = MAX_FINE_AMOUNT;
    
    return fine;
}

// Get total unpaid fines for a user
double GetTotalUnpaidFines(const std::string& username) {
    double total = 0.0;
    for (const auto& h : borrowHistory) {
        if (h.borrowerName == username && !h.finePaid && h.fine > 0.0) {
            total += h.fine;
        }
    }
    return total;
}

// Save fine settings to configuration file
void SaveFineSettings() {
    std::string settingsPath = basePath + "fine_settings.cfg";
    std::ofstream file(settingsPath);
    if (!file.is_open()) {
        std::cout << "ERROR: Failed to save fine settings to " << settingsPath << std::endl;
        return;
    }
    
    file << "LOAN_PERIOD_DAYS=" << LOAN_PERIOD_DAYS << "\n";
    file << "FINE_PER_DAY=" << FINE_PER_DAY << "\n";
    file << "MAX_FINE_AMOUNT=" << MAX_FINE_AMOUNT << "\n";
    
    file.close();
    std::cout << "Fine settings saved successfully." << std::endl;
}

// Load fine settings from configuration file
void LoadFineSettings() {
    std::string settingsPath = basePath + "fine_settings.cfg";
    std::ifstream file(settingsPath);
    if (!file.is_open()) {
        std::cout << "No fine settings file found, using defaults." << std::endl;
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        try {
            if (key == "LOAN_PERIOD_DAYS") {
                int val = std::stoi(value);
                if (val >= 1 && val <= 90) LOAN_PERIOD_DAYS = val;
            } else if (key == "FINE_PER_DAY") {
                double val = std::stod(value);
                if (val >= 0.0 && val <= 50.0) FINE_PER_DAY = val;
            } else if (key == "MAX_FINE_AMOUNT") {
                double val = std::stod(value);
                if (val >= 0.0 && val <= 2000.0) MAX_FINE_AMOUNT = val;
            }
        } catch (...) {
            std::cout << "Error parsing setting: " << line << std::endl;
        }
    }
    
    file.close();
    std::cout << "Fine settings loaded: Loan Period=" << LOAN_PERIOD_DAYS 
              << " days, Fine Rate=₱" << FINE_PER_DAY 
              << "/day, Max Fine=₱" << MAX_FINE_AMOUNT << std::endl;
}

// New: returns true if save succeeded, false if user exceeded limit
// Also ensures unique return code generated for this entry
bool SaveBorrowEntry(const std::string& bookTitle, const std::string& user) {
    // Count active borrows by this user
    int activeBorrows = 0;
    for (const auto& h : borrowHistory) {
        if (h.borrowerName == user && !h.isReturned) activeBorrows++;
    }

    // Enforce 2 books per user rule (allow up to 2 concurrent borrows)
    if (activeBorrows >= 2) {
        // set a UI-friendly message; there's a better place in UI to show it
        return false;
    }

    time_t t = time(nullptr);
    std::tm now = GetLocalTimeSafe(t);
    char dateStr[30];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", &now);

    HistoryEntry newEntry;
    newEntry.bookTitle = bookTitle;
    newEntry.borrowerName = user;
    newEntry.borrowDate = dateStr;
    newEntry.dueDate = CalculateDueDate(dateStr);  // Set due date
    newEntry.isReturned = false;
    newEntry.returnDate = "";
    newEntry.returnCode = GenerateReturnCode();
    newEntry.fine = 0.0;
    newEntry.finePaid = false;

    borrowHistory.push_back(newEntry);
    SaveHistoryToCSV();
    
    // Log the borrow activity
    LogActivity(user, "Borrow", "Borrowed book: " + bookTitle);
    
    return true;
}

bool SaveUsersToCSV() {
    std::ofstream file(usersCSV, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        profileMessage = "ERROR: Cannot write to users.csv. Check file permissions or run as administrator.";
        std::cerr << "Failed to open for writing: " << usersCSV << std::endl;
        return false;
    }

    for (const auto& pair : users) {
        const UserProfile& p = pair.second;
        std::string pass = p.password.empty() ? std::string("changeme") : p.password;
        std::string role = p.role.empty() ? std::string("student") : p.role;
        file << p.username << "," << pass << "," << role << "\n";
    }
    
    file.close();
    if (file.fail()) {
        profileMessage = "ERROR: Failed to save users.csv. Check disk space and permissions.";
        return false;
    }
    
    return true;
}

void SaveUserProfilesToCSV() {
    std::ofstream file(userProfileCSV, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        profileMessage = "ERROR: Cannot write to usersprofile.csv. Check file permissions.";
        std::cerr << "Failed to open for writing: " << userProfileCSV << std::endl;
        return;
    }

    for (const auto& pair : users) {
        const UserProfile& p = pair.second;
        // Format: username, name, section, gender, age
        file << p.username << ","
            << p.name << ","
            << p.section << ","
            << p.gender << ","
            << p.age << "\n";
    }
}

// ================ Activity Logging System ================

void LogActivity(const std::string& username, const std::string& action, const std::string& details) {
    ActivityLog log;
    log.timestamp = GetCurrentDateTimeString();
    log.username = username;
    log.action = action;
    log.details = details;
    
    activityLogs.push_back(log);
    SaveActivityLogsToCSV();
}

void LoadActivityLogsFromCSV() {
    activityLogs.clear();
    std::ifstream file("db/activity_logs.csv");
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::stringstream ss(line);
        std::string timestamp, username, action, details;
        
        std::getline(ss, timestamp, ',');
        std::getline(ss, username, ',');
        std::getline(ss, action, ',');
        std::getline(ss, details); // Rest of line
        
        ActivityLog log;
        log.timestamp = timestamp;
        log.username = username;
        log.action = action;
        log.details = details;
        
        activityLogs.push_back(log);
    }
}

void SaveActivityLogsToCSV() {
    std::ofstream file("db/activity_logs.csv");
    if (!file.is_open()) return;

    for (const auto& log : activityLogs) {
        file << log.timestamp << ","
             << log.username << ","
             << log.action << ","
             << log.details << "\n";
    }
}

void ExportActivityLogsToPDF(const std::string& filename, const std::vector<ActivityLog>& logs) {
    // Create HTML content for PDF conversion
    std::string htmlContent = R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        h1 { color: #333; text-align: center; }
        table { width: 100%; border-collapse: collapse; margin-top: 20px; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #4CAF50; color: white; }
        tr:nth-child(even) { background-color: #f2f2f2; }
        .header { text-align: center; margin-bottom: 20px; }
        .timestamp { color: #666; }
    </style>
</head>
<body>
    <div class="header">
        <h1>FEU E-Library Activity Log Report</h1>
        <p class="timestamp">Generated: )" + GetCurrentDateTimeString() + R"(</p>
    </div>
    <table>
        <tr>
            <th>Timestamp</th>
            <th>Username</th>
            <th>Action</th>
            <th>Details</th>
        </tr>
)";

    for (const auto& log : logs) {
        htmlContent += "        <tr>\n";
        htmlContent += "            <td>" + log.timestamp + "</td>\n";
        htmlContent += "            <td>" + log.username + "</td>\n";
        htmlContent += "            <td>" + log.action + "</td>\n";
        htmlContent += "            <td>" + log.details + "</td>\n";
        htmlContent += "        </tr>\n";
    }

    htmlContent += R"(    </table>
    <div style="margin-top: 30px; text-align: center; color: #666;">
        <p>Total Activities: )" + std::to_string(logs.size()) + R"(</p>
    </div>
</body>
</html>)";

    // Save HTML file
    std::string htmlFilename = filename + ".html";
    std::ofstream htmlFile(htmlFilename);
    if (htmlFile.is_open()) {
        htmlFile << htmlContent;
        htmlFile.close();
        
        // Open the HTML file in the default browser for printing to PDF
        ShellExecuteA(NULL, "open", htmlFilename.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
}

// Create a new user account with basic profile information. Returns true on success.
bool CreateNewUser(const std::string& username, const std::string& password, const std::string& fullName, 
                   const std::string& section, const std::string& gender, int age) {
    if (username.empty() || password.empty()) {
        createAccountMessage = "Username and password are required.";
        return false;
    }

    // Check if username already exists
    if (users.count(username) > 0) {
        createAccountMessage = "Username already exists. Please choose a different username.";
        return false;
    }

    // Validate username (alphanumeric and underscore only)
    for (char c : username) {
        if (!std::isalnum((unsigned char)c) && c != '_') {
            createAccountMessage = "Username can only contain letters, numbers, and underscores.";
            return false;
        }
    }

    // Validate password length
    if (password.length() < 4) {
        createAccountMessage = "Password must be at least 4 characters long.";
        return false;
    }

    // Validate age if provided
    if (age < 0 || age > 150) {
        createAccountMessage = "Please enter a valid age (0-150).";
        return false;
    }

    // Create the new user profile
    UserProfile newUser;
    newUser.username = username;
    newUser.password = password;
    newUser.role = "student"; // New accounts are students by default
    newUser.name = fullName.empty() ? "N/A" : fullName;
    newUser.section = section.empty() ? "N/A" : section;
    newUser.gender = gender.empty() ? "N/A" : gender;
    newUser.age = age;

    // Add to users map
    users[username] = newUser;

    // Save to CSV files
    if (!SaveUsersToCSV()) {
        users.erase(username); // Rollback if save failed
        createAccountMessage = "Failed to save user credentials. Please try again.";
        return false;
    }

    SaveUserProfilesToCSV();
    
    // Log the user creation activity
    LogActivity(username, "Create User", "New user account created: " + username + " (" + fullName + ")");
    
    createAccountMessage = "Account created successfully! You can now log in.";
    return true;
}

// Remove a user from users.csv, profiles, and related caches. Returns true on success.
bool RemoveUser(const std::string& username) {
    if (username.empty()) return false;

    auto it = users.find(username);
    if (it == users.end()) return false;

    UserProfile backup = it->second;
    users.erase(it);

    if (!SaveUsersToCSV()) {
        users[username] = backup; // restore so UI stays accurate if persistence failed
        return false;
    }

    SaveUserProfilesToCSV();
    if (userFavorites.erase(username) > 0) {
        SaveFavoritesToCSV();
    }
    
    // Log the user deletion activity
    LogActivity("admin", "Delete User", "Deleted user account: " + username);

    return true;
}

// Update a user's credentials (password and/or role) in users.csv.
// If newPass is empty, preserve the existing password.
void UpdateUserCredentials(const std::string& username, const std::string& newPass, const std::string& newRole) {
    if (username.empty()) return;

    std::vector<std::tuple<std::string,std::string,std::string>> entries;
    try {
        std::ifstream in(usersCSV);
        if (in.is_open()) {
            std::string line;
            while (std::getline(in, line)) {
                std::stringstream ss(line);
                std::string user_str, pass_str, role_str;
                if (std::getline(ss, user_str, ',') && std::getline(ss, pass_str, ',') && std::getline(ss, role_str)) {
                    entries.emplace_back(trim(user_str), pass_str, trim(role_str));
                }
            }
            in.close();
        }

        bool found = false;
        for (auto &t : entries) {
            if (std::get<0>(t) == username) {
                // preserve password if newPass empty
                if (!newPass.empty()) std::get<1>(t) = newPass;
                if (!newRole.empty()) std::get<2>(t) = newRole;
                found = true;
                break;
            }
        }

        // If user wasn't present, append it
        if (!found) {
            entries.emplace_back(username, newPass, newRole.empty() ? std::string("student") : newRole);
        }

        // Write back
        std::ofstream out(usersCSV);
        if (out.is_open()) {
            for (const auto &t : entries) {
                out << std::get<0>(t) << "," << std::get<1>(t) << "," << std::get<2>(t) << "\n";
            }
            out.close();
        }

        // Reload in-memory users (and profiles) so UI reflects changes
        LoadUsersFromCSV();
    }
    catch (...) {
        // best-effort
    }
}

// Save current in-memory books to the configured `booksCSV` file
void SaveBooksToCSV() {
    std::ofstream file(booksCSV);
    if (!file.is_open()) return;
    for (const auto &b : books) {
        file << b.title << "," << b.author << "," << b.category << "," << b.iconPath << "\n";
    }
}

// Import a CSV located at `path` into the app: copies to booksCSV and reloads
bool ImportBooksFromCSV(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;
    std::ofstream out(booksCSV);
    if (!out.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        out << line << "\n";
    }
    out.close();
    LoadBooksFromCSV();
    return true;
}

// Populates extended profile fields (name/section/etc) onto existing users
void LoadUserProfilesFromCSV() {
    std::ifstream file(userProfileCSV);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string user_str, name_str, section_str, gender_str, age_str;

        if (std::getline(ss, user_str, ',') &&
            std::getline(ss, name_str, ',') &&
            std::getline(ss, section_str, ',') &&
            std::getline(ss, gender_str, ',') &&
            std::getline(ss, age_str)) {

            std::string username = trim(user_str);
            if (users.count(username)) {
                // Attach profile details to the existing user loaded from users.csv
                users[username].name = trim(name_str);
                users[username].section = trim(section_str);
                users[username].gender = trim(gender_str);
                try {
                    users[username].age = std::stoi(trim(age_str));
                }
                catch (...) {
                    users[username].age = 0;
                }
            }
        }
    }
}

// Primary user map loader: parses credentials then chains to profile loader
void LoadUsersFromCSV() {
    users.clear();
    std::ifstream file(usersCSV);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string user_str, pass_str, role_str;
        UserProfile p;

        if (std::getline(ss, user_str, ',') &&
            std::getline(ss, pass_str, ',') &&
            std::getline(ss, role_str)) {

            p.username = trim(user_str);
            p.password = trim(pass_str);
            p.role = trim(role_str);

            if (!p.username.empty()) {
                users[p.username] = p;
            }
        }
    }
    LoadUserProfilesFromCSV();
}

UserProfile VerifyUserCredentials(const char* username_char, const char* password_char) {
    std::ifstream file(usersCSV);
    if (!file.is_open()) { loginError = "User file not found."; return UserProfile{}; }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string user_str, pass_str, role_str;

        if (std::getline(ss, user_str, ',') &&
            std::getline(ss, pass_str, ',') &&
            std::getline(ss, role_str)) {

            user_str = trim(user_str);
            pass_str = trim(pass_str);
            role_str = trim(role_str);

            if (user_str == username_char && pass_str == password_char) {
                loginError = "";
                if (users.count(user_str)) {
                    return users[user_str];
                }
                UserProfile p;
                p.username = user_str;
                p.role = role_str;
                return p;
            }
        }
    }
    loginError = "Invalid username or password.";
    return UserProfile{};
}

void ReturnBook(const std::string& bookTitle, const std::string& username, const std::string& returnCode) {
    bool found = false;
    for (int i = borrowHistory.size() - 1; i >= 0; --i) {
        auto& h = borrowHistory[i];
        if (h.bookTitle == bookTitle && h.borrowerName == username && !h.isReturned) {

            if (h.returnCode == returnCode) {
                time_t t = time(nullptr);
                std::tm now = GetLocalTimeSafe(t);
                char dateStr[30];
                strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", &now);

                h.isReturned = true;
                h.returnDate = dateStr;
                
                // Calculate fine if overdue
                if (!h.dueDate.empty()) {
                    h.fine = CalculateFine(h.dueDate, h.returnDate);
                }
                
                // Log the return activity
                std::string fineInfo = (h.fine > 0) ? " (Fine: ₱" + std::to_string((int)h.fine) + ")" : "";
                LogActivity(username, "Return", "Returned book: " + bookTitle + fineInfo);
                
                found = true;
                break;
            }
        }
    }
    if (found) {
        SaveHistoryToCSV();
    }
}

// Admin helper: mark a history entry returned by index (used in Admin UI)
void AdminMarkEntryReturned(int index) {
    if (index < 0 || index >= (int)borrowHistory.size()) return;
    HistoryEntry &h = borrowHistory[index];
    if (h.isReturned) return;
    time_t t = time(nullptr);
    std::tm now = GetLocalTimeSafe(t);
    char dateStr[30];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", &now);
    h.isReturned = true;
    h.returnDate = dateStr;
    
    // Calculate fine if overdue
    if (!h.dueDate.empty()) {
        h.fine = CalculateFine(h.dueDate, h.returnDate);
    }
    
    SaveHistoryToCSV();
    
    std::string msg = std::string("Marked returned: ") + h.bookTitle + " for " + h.borrowerName;
    if (h.fine > 0.0) {
        msg += std::string(" (Fine: ₱") + std::to_string((int)h.fine) + ")";
    }
    profileMessage = msg;
}

// Copy book cover to icons folder and return relative path
std::string CopyBookCoverToIcons(const std::string& sourcePath) {
    if (sourcePath.empty()) return "";
    
    // Check if already a relative path (starts with "icons/")
    if (sourcePath.find("icons/") == 0 || sourcePath.find("icons\\") == 0) {
        return sourcePath;
    }
    
    // Extract filename from absolute path
    std::string filename = sourcePath;
    size_t lastSlash = sourcePath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        filename = sourcePath.substr(lastSlash + 1);
    }
    
    // Create destination path in icons folder
    std::string destPath = "icons/" + filename;
    std::string fullDestPath = basePath + "..\\..\\" + destPath;
    
    // Ensure icons directory exists
    std::string iconsDir = basePath + "..\\..\\icons";
    _mkdir(iconsDir.c_str());
    
    // Copy file
    if (CopyFileA(sourcePath.c_str(), fullDestPath.c_str(), FALSE)) {
        return destPath; // Return relative path
    }
    
    // If copy failed, return original path
    return sourcePath;
}

// Remove a book by title (releases textures and persists to CSV)
void RemoveBook(const std::string& title) {
    if (title.empty()) return;
    // find by title
    for (int i = 0; i < (int)books.size(); ++i) {
        if (books[i].title == title) {
            // release associated texture if cached
            try {
                std::string key = books[i].iconPath;
                if (!key.empty() && g_bookTextures.count(key)) {
                    ID3D11ShaderResourceView* srv = g_bookTextures[key];
                    if (srv) srv->Release();
                    g_bookTextures.erase(key);
                    g_bookTextureSizes.erase(key);
                }
            } catch(...) {}
            // erase from vector
            books.erase(books.begin() + i);
            SaveBooksToCSV();
            LoadBooksFromCSV();
            
            // Log the book removal activity
            LogActivity("admin", "Remove Book", "Removed book: " + title);
            
            profileMessage = std::string("Removed book: ") + title;
            return;
        }
    }
}

bool IsBookBorrowed(const std::string& title) {
    for (auto& h : borrowHistory)
        if (h.bookTitle == title && !h.isReturned)
            return true;
    return false;
}

// Returns true if `title` is currently borrowed by `username` and not yet returned
bool IsBookBorrowedBy(const std::string& title, const std::string& username) {
    for (const auto& h : borrowHistory) {
        if (h.bookTitle == title && h.borrowerName == username && !h.isReturned) return true;
    }
    return false;
}

static bool ParseBooksCSV(const std::string& path, std::vector<Book>& outBooks) {
    outBooks.clear();

    // Debug log for cover/icon path resolution (optional)
    char exeBufLocal[MAX_PATH] = { 0 };
    std::string exeDirLocal;
    if (GetModuleFileNameA(NULL, exeBufLocal, MAX_PATH) != 0) {
        exeDirLocal = std::filesystem::path(exeBufLocal).parent_path().string();
    }
    std::ofstream dbgLog;
    if (!exeDirLocal.empty()) dbgLog.open(exeDirLocal + "\\cover_load.log", std::ios::app);
    else dbgLog.open("cover_load.log", std::ios::app);
    if (dbgLog.is_open()) dbgLog << "ParseBooksCSV path=" << path << std::endl;

    std::ifstream file(path);
    if (!file.is_open()) {
        if (dbgLog.is_open()) {
            dbgLog << "ParseBooksCSV: FAILED to open " << path << std::endl;
            dbgLog.flush();
        }
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        Book b;
        std::string iconPath_temp;
        if (std::getline(ss, b.title, ',') &&
            std::getline(ss, b.author, ',') &&
            std::getline(ss, b.category, ',') &&
            std::getline(ss, iconPath_temp)) {

            b.title = trim(b.title);
            b.author = trim(b.author);
            b.category = trim(b.category);
            b.iconPath = trim(iconPath_temp);

            // Normalize and attempt to map icon paths to the repository icons folder when
            // running from Release/. If the icon doesn't exist at the configured path,
            // try to find a matching file under ../icons/ using a sanitized title.
            try {
                char buf[MAX_PATH] = { 0 };
                if (GetModuleFileNameA(NULL, buf, MAX_PATH) != 0) {
                    std::filesystem::path exeP(buf);
                    std::filesystem::path repoExampleDir = exeP.parent_path().parent_path();
                    std::filesystem::path iconsDir = repoExampleDir / "icons";

                    std::filesystem::path given = b.iconPath;
                    std::filesystem::path candidate = iconsDir / given.filename();
                    if (std::filesystem::exists(candidate)) {
                        b.iconPath = std::string("icons/") + candidate.filename().string();
                    }
                    else {
                        std::string san;
                        for (char c : b.title) {
                            if (std::isalnum((unsigned char)c)) san.push_back(c);
                            else if (std::isspace((unsigned char)c)) san.push_back('_');
                        }
                        if (!san.empty()) {
                            std::filesystem::path alt = iconsDir / (san + ".jpg");
                            if (std::filesystem::exists(alt)) {
                                b.iconPath = std::string("icons/") + alt.filename().string();
                            }
                            else {
                                alt = iconsDir / (san + ".png");
                                if (std::filesystem::exists(alt)) b.iconPath = std::string("icons/") + alt.filename().string();
                            }
                        }
                    }
                }
            }
            catch (...) {
                // ignore mapping failures and keep the original iconPath
            }

            try {
                std::string resolved = b.iconPath;
                bool exists = false;
                if (!exeDirLocal.empty()) {
                    std::filesystem::path cand1 = std::filesystem::path(exeDirLocal) / b.iconPath;
                    std::filesystem::path cand2 = std::filesystem::path(exeDirLocal).parent_path() / "icons" / std::filesystem::path(b.iconPath).filename();
                    exists = std::filesystem::exists(cand1) || std::filesystem::exists(cand2) || std::filesystem::exists(b.iconPath);
                } else {
                    exists = std::filesystem::exists(b.iconPath);
                }
                if (dbgLog.is_open()) {
                    dbgLog << "Book: '" << b.title << "' -> iconPath='" << resolved << "' exists=" << (exists?"1":"0") << std::endl;
                    dbgLog.flush();
                }
            }
            catch (...) { }

            outBooks.push_back(b);
        }
    }

    return true;
}

// Rebuilds the in-memory book list and derived categories from CSV
void LoadBooksFromCSV() {
    std::vector<Book> parsed;
    if (!ParseBooksCSV(booksCSV, parsed)) {
        books.clear();
        categories = { "All" };
        return;
    }

    books = parsed;
    categories = { "All" };
    for (const auto& b : books) {
        if (std::find(categories.begin(), categories.end(), b.category) == categories.end()) {
            categories.push_back(b.category);
        }
    }

    PruneFavoritesToExistingBooks();
}

bool LoadBooksFromCSVPath(const std::string& path, std::vector<Book>& outBooks) {
    return ParseBooksCSV(path, outBooks);
}

void PruneFavoritesToExistingBooks() {
    std::unordered_set<std::string> validTitles;
    validTitles.reserve(books.size());
    for (const auto& b : books) {
        validTitles.insert(b.title);
    }

    bool changed = false;
    for (auto userIt = userFavorites.begin(); userIt != userFavorites.end(); ) {
        auto& favs = userIt->second;
        for (auto favIt = favs.begin(); favIt != favs.end(); ) {
            if (!validTitles.count(*favIt)) {
                favIt = favs.erase(favIt);
                changed = true;
            } else {
                ++favIt;
            }
        }
        if (favs.empty()) {
            userIt = userFavorites.erase(userIt);
            changed = true;
        } else {
            ++userIt;
        }
    }

    if (changed) SaveFavoritesToCSV();
}

// Reload favorites.csv into the username->titles set map
void LoadFavoritesFromCSV() {
    userFavorites.clear();

    std::ifstream file(favoritesCSV);
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string username;
        std::string title;
        if (std::getline(ss, username, ',') && std::getline(ss, title)) {
            username = trim(username);
            title = trim(title);
            if (!username.empty() && !title.empty()) {
                userFavorites[username].insert(title);
            }
        }
    }

    PruneFavoritesToExistingBooks();
}

void SaveFavoritesToCSV() {
    std::ofstream file(favoritesCSV);
    if (!file.is_open()) return;

    for (const auto& kv : userFavorites) {
        for (const auto& title : kv.second) {
            file << kv.first << "," << title << "\n";
        }
    }
}

bool IsBookFavorited(const std::string& username, const std::string& title) {
    if (username.empty()) return false;
    auto it = userFavorites.find(username);
    if (it == userFavorites.end()) return false;
    return it->second.count(title) > 0;
}

// Shared toggle used by both student/admin watchlists
void ToggleFavorite(const std::string& username, const std::string& title) {
    if (username.empty() || title.empty()) return;
    auto& favs = userFavorites[username];
    if (favs.count(title)) {
        favs.erase(title);
        if (favs.empty()) {
            userFavorites.erase(username);
        }
    } else {
        favs.insert(title);
    }
    SaveFavoritesToCSV();
}

std::vector<std::string> GetFavoritesForUser(const std::string& username) {
    std::vector<std::string> result;
    if (username.empty()) return result;
    auto it = userFavorites.find(username);
    if (it == userFavorites.end()) return result;
    result.assign(it->second.begin(), it->second.end());
    std::sort(result.begin(), result.end());
    return result;
}

int GetFavoriteCountForBook(const std::string& title) {
    if (title.empty()) return 0;
    int count = 0;
    for (const auto& kv : userFavorites) {
        if (kv.second.count(title)) count++;
    }
    return count;
}

const char* BookDiffTypeLabel(BookDiffType type) {
    switch (type) {
    case BookDiffType::Added:   return "Added";
    case BookDiffType::Removed: return "Removed";
    case BookDiffType::Updated: return "Updated";
    default: return "Unknown";
    }
}

ImVec4 BookDiffTypeColor(BookDiffType type) {
    switch (type) {
    case BookDiffType::Added:   return ImVec4(0.20f, 0.80f, 0.35f, 1.0f);
    case BookDiffType::Removed: return ImVec4(0.92f, 0.40f, 0.30f, 1.0f);
    case BookDiffType::Updated: return ImVec4(0.95f, 0.74f, 0.25f, 1.0f);
    default: return ImVec4(0.80f, 0.80f, 0.80f, 1.0f);
    }
}

BookDiffResult ComputeBookDiff(const std::vector<Book>& currentList, const std::vector<Book>& incomingList) {
    BookDiffResult result;

    std::unordered_map<std::string, Book> currentByTitle;
    currentByTitle.reserve(currentList.size());
    for (const auto& b : currentList) {
        if (!b.title.empty()) currentByTitle[b.title] = b;
    }

    std::unordered_map<std::string, Book> incomingByTitle;
    incomingByTitle.reserve(incomingList.size());
    for (const auto& b : incomingList) {
        if (!b.title.empty()) incomingByTitle[b.title] = b;
    }

    for (const auto& pair : incomingByTitle) {
        auto it = currentByTitle.find(pair.first);
        if (it == currentByTitle.end()) {
            BookDiffEntry entry;
            entry.incomingBook = pair.second;
            entry.type = BookDiffType::Added;
            result.entries.push_back(entry);
            result.added++;
        } else {
            const Book& cur = it->second;
            const Book& incoming = pair.second;
            if (cur.author != incoming.author || cur.category != incoming.category || cur.iconPath != incoming.iconPath) {
                BookDiffEntry entry;
                entry.currentBook = cur;
                entry.incomingBook = incoming;
                entry.type = BookDiffType::Updated;
                result.entries.push_back(entry);
                result.updated++;
            }
        }
    }

    for (const auto& pair : currentByTitle) {
        if (!incomingByTitle.count(pair.first)) {
            BookDiffEntry entry;
            entry.currentBook = pair.second;
            entry.type = BookDiffType::Removed;
            result.entries.push_back(entry);
            result.removed++;
        }
    }

    auto typeRank = [](BookDiffType t) {
        switch (t) {
        case BookDiffType::Added: return 0;
        case BookDiffType::Updated: return 1;
        case BookDiffType::Removed: return 2;
        default: return 3;
        }
    };

    std::sort(result.entries.begin(), result.entries.end(), [&](const BookDiffEntry& a, const BookDiffEntry& b) {
        int ra = typeRank(a.type);
        int rb = typeRank(b.type);
        if (ra != rb) return ra < rb;
        const std::string& titleA = (a.type == BookDiffType::Removed) ? a.currentBook.title : a.incomingBook.title;
        const std::string& titleB = (b.type == BookDiffType::Removed) ? b.currentBook.title : b.incomingBook.title;
        return titleA < titleB;
    });

    return result;
}


// ---------------- UI Rendering Functions ----------------

void RenderLoginScreen(float fullSizeX, float fullSizeY, bool& loggedIn, char* username, char* password) {
    // Toggle between login and create account modes
    if (!showCreateAccount) {
        // LOGIN MODE
        ImGui::SetNextWindowPos(ImVec2(fullSizeX * 0.5f - 220.0f, fullSizeY * 0.5f - 170.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(440.0f, 340.0f), ImGuiCond_Always);
        if (!ImGui::Begin("E-Library Login", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar))
        {
            ImGui::End();
            return;
        }

        ImGui::Text("Welcome to the FEU E-Library!");
        ImGui::TextDisabled("Sign in to browse and borrow books");
        ImGui::Separator();

        ImGui::Spacing();
        ImGui::InputTextWithHint("##login_user", "Username", username, 64);
        ImGui::Spacing();

        static bool showPassword = false;
        ImGui::InputTextWithHint("##login_pass", "Password", password, 64,
            showPassword ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_Password);
        ImGui::SameLine();
        ImGui::Checkbox("Show", &showPassword);

        ImGui::Spacing();
        bool attemptLogin = false;
        if (ImGui::Button("Sign In", ImVec2(-1, 36))) {
            attemptLogin = true;
        }

        // Allow pressing Enter anywhere inside the login card to submit
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            attemptLogin = true;
        }

        if (attemptLogin) {
            UserProfile verified = VerifyUserCredentials(username, password);
            if (!verified.username.empty()) {
                // ensure we have the most recent profile info from the users map
                if (users.count(verified.username)) {
                    currentUser = users[verified.username];
                } else {
                    currentUser = verified;
                }
                loggedIn = true;
                loginError.clear();
                memset(password, 0, 64);
                forceProfileBufferReload = true;
                showProfilePopup = false;
                
                // Log the login activity
                LogActivity(currentUser.username, "Login", "User logged in");
            }
        }

        if (!loginError.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", loginError.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Add "Create Account" button
        if (ImGui::Button("Create New Account", ImVec2(-1, 32))) {
            showCreateAccount = true;
            createAccountMessage.clear();
            memset(newUsername, 0, sizeof(newUsername));
            memset(newPassword, 0, sizeof(newPassword));
            memset(newPasswordConfirm, 0, sizeof(newPasswordConfirm));
            memset(newFullName, 0, sizeof(newFullName));
            memset(newSection, 0, sizeof(newSection));
            memset(newGender, 0, sizeof(newGender));
            newAge = 0;
        }

        ImGui::End();
    }
    else {
        // CREATE ACCOUNT MODE
        ImGui::SetNextWindowPos(ImVec2(fullSizeX * 0.5f - 260.0f, fullSizeY * 0.5f - 280.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(520.0f, 560.0f), ImGuiCond_Always);
        if (!ImGui::Begin("Create Account", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar))
        {
            ImGui::End();
            return;
        }

        ImGui::Text("Create Your Account");
        ImGui::TextDisabled("Register for access to the E-Library");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Username:");
        ImGui::InputTextWithHint("##new_user", "Choose a username", newUsername, 64);
        ImGui::TextDisabled("(letters, numbers, and underscores only)");
        ImGui::Spacing();

        ImGui::Text("Full Name:");
        ImGui::InputTextWithHint("##new_name", "Your full name", newFullName, 128);
        ImGui::Spacing();

        ImGui::Text("Section:");
        ImGui::InputTextWithHint("##new_section", "e.g., TN04, TN06", newSection, 64);
        ImGui::Spacing();

        ImGui::Text("Gender:");
        ImGui::InputTextWithHint("##new_gender", "e.g., Male, Female, Other", newGender, 32);
        ImGui::Spacing();

        ImGui::Text("Age:");
        ImGui::InputInt("##new_age", &newAge, 1, 100);
        if (newAge < 0) newAge = 0;
        if (newAge > 150) newAge = 150;
        ImGui::Spacing();

        ImGui::Text("Password:");
        static bool showNewPassword = false;
        ImGui::InputTextWithHint("##new_pass", "Choose a password", newPassword, 64,
            showNewPassword ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_Password);
        ImGui::TextDisabled("(minimum 4 characters)");
        ImGui::Spacing();

        ImGui::Text("Confirm Password:");
        ImGui::InputTextWithHint("##new_pass_confirm", "Re-enter password", newPasswordConfirm, 64,
            showNewPassword ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_Password);
        ImGui::Checkbox("Show Password", &showNewPassword);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool attemptCreate = false;
        if (ImGui::Button("Create Account", ImVec2(-1, 36))) {
            attemptCreate = true;
        }

        // Allow pressing Enter to submit
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            attemptCreate = true;
        }

        if (attemptCreate) {
            // Validate password confirmation
            std::string pass1 = newPassword;
            std::string pass2 = newPasswordConfirm;
            
            if (pass1 != pass2) {
                createAccountMessage = "Passwords do not match. Please try again.";
            } else {
                // Attempt to create the account
                bool success = CreateNewUser(newUsername, newPassword, newFullName, newSection, newGender, newAge);
                if (success) {
                    // Clear the form
                    memset(newUsername, 0, sizeof(newUsername));
                    memset(newPassword, 0, sizeof(newPassword));
                    memset(newPasswordConfirm, 0, sizeof(newPasswordConfirm));
                    memset(newFullName, 0, sizeof(newFullName));
                    memset(newSection, 0, sizeof(newSection));
                    memset(newGender, 0, sizeof(newGender));
                    newAge = 0;
                }
            }
        }

        if (!createAccountMessage.empty()) {
            ImGui::Spacing();
            bool isError = (createAccountMessage.find("successfully") == std::string::npos);
            ImVec4 msgColor = isError ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f) : ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
            ImGui::TextColored(msgColor, "%s", createAccountMessage.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Back to Login", ImVec2(-1, 32))) {
            showCreateAccount = false;
            createAccountMessage.clear();
            loginError.clear();
        }

        ImGui::End();
    }
}

// --- PROFILE INFO DISPLAY ---
void RenderProfilePopup(float fullSizeX, float fullSizeY, UserProfile& user) {
    // Admin View ONLY USER PROFILE
    bool isAdmin = (currentUser.role == "admin");
    bool viewingOther = (!user.username.empty() && user.username != currentUser.username);
    bool readOnly = viewingOther; // Always read-only when viewing another user's profile

    // Debug output
    std::cout << "=== RenderProfilePopup ===" << std::endl;
    std::cout << "User: " << user.username << std::endl;
    std::cout << "isAdmin: " << isAdmin << std::endl;
    std::cout << "viewingOther: " << viewingOther << std::endl;
    std::cout << "readOnly: " << readOnly << std::endl;

    if (users.find(user.username) == users.end()) {
        profileMessage = "ERROR: Could not find user profile in database map.";
        showProfilePopup = false;
        return;
    }

    static bool first_open_load = true;
    static char editPasswordBuffer[128] = "";
    static int editRoleIdx = 0; // 0=student,1=admin

    if (showProfilePopup && (first_open_load || forceProfileBufferReload)) {
        // Always get fresh reference from map when reloading
        UserProfile& freshProfile = users[user.username];
        
        // Debug output
        std::cout << "Loading profile for: " << user.username << std::endl;
        std::cout << "  From DB - Name: " << freshProfile.name << std::endl;
        std::cout << "  From DB - Section: " << freshProfile.section << std::endl;
        std::cout << "  From DB - Gender: " << freshProfile.gender << std::endl;
        std::cout << "  From DB - Age: " << freshProfile.age << std::endl;
        
        strncpy_s(profileNameBuffer, sizeof(profileNameBuffer), freshProfile.name.c_str(), _TRUNCATE);
        profileNameBuffer[sizeof(profileNameBuffer) - 1] = '\0';
        strncpy_s(profileSectionBuffer, sizeof(profileSectionBuffer), freshProfile.section.c_str(), _TRUNCATE);
        profileSectionBuffer[sizeof(profileSectionBuffer) - 1] = '\0';
        strncpy_s(profileGenderBuffer, sizeof(profileGenderBuffer), freshProfile.gender.c_str(), _TRUNCATE);
        profileGenderBuffer[sizeof(profileGenderBuffer) - 1] = '\0';
        profileAge = freshProfile.age;
            
            // Only clear message on first open, not on reload after save
            if (first_open_load) {
                profileMessage.clear();
            }

            // password buffer empty by default (admin can set a new password)
            editPasswordBuffer[0] = '\0';
            editRoleIdx = (freshProfile.role == "admin") ? 1 : 0;

            first_open_load = false;
            forceProfileBufferReload = false;
        }

    if (!showProfilePopup) {
        first_open_load = true;
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(500, 350), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(fullSizeX / 2 - 250, fullSizeY / 2 - 175), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    const char* windowTitle = readOnly ? "View User Profile (Read-Only)" : "Edit User Profile";

    if (ImGui::Begin(windowTitle, &showProfilePopup,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar)) {

        if (ImGui::IsMouseReleased(0) &&
            !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) &&
            !ImGui::IsAnyItemActive())
        {
            showProfilePopup = false;
            // if admin was viewing another user's profile, clear selection on close
            if (readOnly) profileViewUser = UserProfile{};
        }

        ImGui::Text("User Account: %s (%s)", user.username.c_str(), user.role.c_str());
        ImGui::Separator();
        ImGui::Text("Personal Information ");
        ImGui::Spacing();

        // If readOnly use disabled state 
#if (IMGUI_VERSION_NUM >= 18700)
        if (readOnly) ImGui::BeginDisabled();
#endif


        ImGui::Text("Full Name:");
        ImGui::InputText("##Name", profileNameBuffer, sizeof(profileNameBuffer), readOnly ? ImGuiInputTextFlags_ReadOnly : 0);

        ImGui::Text("Section:");
        ImGui::InputText("##Section", profileSectionBuffer, sizeof(profileSectionBuffer), readOnly ? ImGuiInputTextFlags_ReadOnly : 0);

        ImGui::Text("Gender:");
        ImGui::InputText("##Gender", profileGenderBuffer, sizeof(profileGenderBuffer), readOnly ? ImGuiInputTextFlags_ReadOnly : 0);

        ImGui::Text("Age:");
        if (readOnly) {

            ImGui::Text("%d", profileAge);
        }
        else {
            ImGui::InputInt("##Age", &profileAge, 1, 100);
            if (profileAge < 0) profileAge = 0;
        }

        // Show account settings only when user is editing their own profile
        bool showCredControls = !viewingOther;

        if (showCredControls) {
            ImGui::Separator();
            ImGui::Text("Account Settings");
            // Password change (empty = keep existing)
            ImGui::InputText("New Password (leave empty to keep)", editPasswordBuffer, IM_ARRAYSIZE(editPasswordBuffer), ImGuiInputTextFlags_Password);
            // Role control (admins only)
            if (isAdmin) {
                ImGui::Combo("Role", &editRoleIdx, "student\0admin\0");
            }
        }

#if (IMGUI_VERSION_NUM >= 18700)
        if (readOnly) ImGui::EndDisabled();
#endif

        ImGui::Separator();

        if (!profileMessage.empty()) {
            ImGui::TextColored(profileMessage.find("ERROR") != std::string::npos ? ImVec4(1, 0.2f, 0.2f, 1) : ImVec4(0.2f, 1, 0.2f, 1), "%s", profileMessage.c_str());
        }

        // Only show Save on editable (user editing their own profile or admin editing)
        if (!readOnly) {
            if (ImGui::Button("Save Changes", ImVec2(150, 30))) {
                if (users.count(user.username)) {
                    // Debug: Check what's in the buffers BEFORE saving
                    std::cout << "=== BEFORE SAVE ===" << std::endl;
                    std::cout << "Username: " << user.username << std::endl;
                    std::cout << "Buffer Name: '" << profileNameBuffer << "'" << std::endl;
                    std::cout << "Buffer Section: '" << profileSectionBuffer << "'" << std::endl;
                    std::cout << "Buffer Gender: '" << profileGenderBuffer << "'" << std::endl;
                    std::cout << "Buffer Age: " << profileAge << std::endl;
                    
                    // Update the profile data directly in the users map
                    users[user.username].name = std::string(profileNameBuffer);
                    users[user.username].section = std::string(profileSectionBuffer);
                    users[user.username].gender = std::string(profileGenderBuffer);
                    users[user.username].age = profileAge;

                    // Debug output
                    std::cout << "=== AFTER UPDATE IN MAP ===" << std::endl;
                    std::cout << "Saving profile for: " << user.username << std::endl;
                    std::cout << "  Name: " << profileNameBuffer << " -> " << users[user.username].name << std::endl;
                    std::cout << "  Section: " << profileSectionBuffer << " -> " << users[user.username].section << std::endl;
                    std::cout << "  Gender: " << profileGenderBuffer << " -> " << users[user.username].gender << std::endl;
                    std::cout << "  Age: " << profileAge << " -> " << users[user.username].age << std::endl;

                    // User editing own profile: allow changing own password if provided
                    if (!viewingOther) {
                        std::string newPass = std::string(editPasswordBuffer);
                        if (!newPass.empty()) {
                            UpdateUserCredentials(user.username, newPass, "");
                        }
                    }

                    // Save to CSV
                    SaveUserProfilesToCSV();
                    
                    // Update the reference that was passed in
                    user = users[user.username];
                    
                    // If editing own profile, update currentUser
                    if (user.username == currentUser.username) {
                        currentUser = users[user.username];
                    }
                    
                    profileMessage = "Profile saved successfully!";
                    
                    // Reload buffers with saved values on next render
                    forceProfileBufferReload = true;
                }
                else {
                    profileMessage = "ERROR: User not found in database map during save operation.";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Close", ImVec2(120, 30))) {
                showProfilePopup = false;
                profileMessage.clear();
                if (readOnly) profileViewUser = UserProfile{};
            }
        }

        ImGui::End();
    }
    ImGui::PopStyleVar();
}

// Settings Popup for Fine/Penalty System Configuration (Admin Only)
void RenderSettingsPopup(float fullSizeX, float fullSizeY) {
    if (!showSettingsPopup) return;

    ImGui::SetNextWindowSize(ImVec2(480, 400), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(fullSizeX / 2 - 240, fullSizeY / 2 - 200), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    if (ImGui::Begin("Fine System Settings", &showSettingsPopup,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar)) {

        if (ImGui::IsMouseReleased(0) &&
            !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) &&
            !ImGui::IsAnyItemActive())
        {
            showSettingsPopup = false;
        }

        ImGui::Text("Fine & Penalty System Settings");
        ImGui::TextDisabled("Configure loan periods and fine rates");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Loan Period (Days):");
        ImGui::SliderInt("##loanperiod", &tempLoanPeriod, 1, 90);
        ImGui::TextDisabled("How many days students can borrow books (1-90 days)");
        ImGui::Spacing();

        ImGui::Text("Fine Per Day (₱):");
        ImGui::InputFloat("##fineperday", &tempFinePerDay, 1.0f, 10.0f, "₱%.2f");
        ImGui::TextDisabled("Daily fine amount for overdue books (₱0-₱50)");
        ImGui::Spacing();

        ImGui::Text("Maximum Fine Cap (₱):");
        ImGui::InputFloat("##maxfine", &tempMaxFine, 10.0f, 100.0f, "₱%.2f");
        ImGui::TextDisabled("Maximum total fine per book (₱0-₱2000)");
        ImGui::Spacing();

        ImGui::Separator();
        ImGui::Spacing();

        // Preview calculation
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.18f, 0.22f, 0.95f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
        if (ImGui::BeginChild("PreviewCalc", ImVec2(0, 80), true, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::TextDisabled("Example:");
            int exampleOverdue = 7;
            float exampleFine = exampleOverdue * tempFinePerDay;
            if (exampleFine > tempMaxFine) exampleFine = tempMaxFine;
            ImGui::Text("Book overdue by %d days = ₱%.2f fine", exampleOverdue, exampleFine);
            ImGui::TextDisabled("Due date: %d days after borrowing", tempLoanPeriod);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        ImGui::Spacing();

        if (!settingsMessage.empty()) {
            bool isError = (settingsMessage.find("Error") != std::string::npos);
            ImVec4 msgColor = isError ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f) : ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
            ImGui::TextColored(msgColor, "%s", settingsMessage.c_str());
            ImGui::Spacing();
        }

        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Save Settings", ImVec2(150, 32))) {
            // Validate and save
            if (tempLoanPeriod < 1 || tempLoanPeriod > 90) {
                settingsMessage = "Error: Loan period must be 1-90 days.";
            } else if (tempFinePerDay < 0.0f || tempFinePerDay > 50.0f) {
                settingsMessage = "Error: Fine per day must be ₱0-₱50.";
            } else if (tempMaxFine < 0.0f || tempMaxFine > 2000.0f) {
                settingsMessage = "Error: Maximum fine must be ₱0-₱2000.";
            } else {
                LOAN_PERIOD_DAYS = tempLoanPeriod;
                FINE_PER_DAY = (double)tempFinePerDay;
                MAX_FINE_AMOUNT = (double)tempMaxFine;
                SaveFineSettings();
                settingsMessage = "Settings saved successfully!";
            }
        }
        ImGui::SameLine();

        if (ImGui::Button("Reset to Defaults", ImVec2(150, 32))) {
            tempLoanPeriod = 14;
            tempFinePerDay = 5.0f;
            tempMaxFine = 500.0f;
            settingsMessage = "Reset to default values.";
        }
        ImGui::SameLine();

        if (ImGui::Button("Close", ImVec2(150, 32))) {
            showSettingsPopup = false;
            settingsMessage.clear();
        }

        ImGui::End();
    }
    ImGui::PopStyleVar();
}


// Book Popup Function
void RenderBookDetailsPopup(float fullSizeX, float fullSizeY, const char* currentUsername) {
    if (!showBookDetailsPopup || selectedBookTitle.empty()) return;

    auto it = std::find_if(books.begin(), books.end(), [&](const Book& b) {
        return b.title == selectedBookTitle;
        });

    ImGui::SetNextWindowSize(ImVec2(540, 360), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(fullSizeX * 0.5f - 270.0f, fullSizeY * 0.5f - 180.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(420, 320), ImVec2(fullSizeX * 0.9f, fullSizeY * 0.9f));

    const std::string currentUserStr = currentUsername ? currentUsername : "";
    bool isBorrowedGlobally = IsBookBorrowed(selectedBookTitle);
    bool isBorrowedByUser = !currentUserStr.empty() && IsBookBorrowedBy(selectedBookTitle, currentUserStr);
    bool isFavorited = !currentUserStr.empty() && IsBookFavorited(currentUserStr, selectedBookTitle);
    int watchCount = GetFavoriteCountForBook(selectedBookTitle);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22, 18));
    if (ImGui::Begin(("Book: " + selectedBookTitle).c_str(), &showBookDetailsPopup,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {


        if (ImGui::IsMouseReleased(0) &&
            !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) &&
            !ImGui::IsAnyItemActive())
        {
            showBookDetailsPopup = false;
        }

        if (it != books.end()) {
            ID3D11ShaderResourceView* coverSrv = nullptr;
            int texW = 0;
            int texH = 0;
            if (!it->iconPath.empty()) {
                auto cached = g_bookTextures.find(it->iconPath);
                if (cached != g_bookTextures.end()) {
                    coverSrv = cached->second;
                    auto sizeIt = g_bookTextureSizes.find(it->iconPath);
                    if (sizeIt != g_bookTextureSizes.end()) {
                        texW = sizeIt->second.first;
                        texH = sizeIt->second.second;
                    }
                }
                else if (LoadTextureFromFile(it->iconPath.c_str(), &coverSrv, &texW, &texH)) {
                    g_bookTextures[it->iconPath] = coverSrv;
                    g_bookTextureSizes[it->iconPath] = { texW, texH };
                }
            }

            auto coverSizeFor = [](int w, int h) {
                const float maxW = 210.0f;
                const float maxH = 300.0f;
                if (w <= 0 || h <= 0) return ImVec2(maxW, maxW * 1.45f);
                float aspect = (float)w / (float)h;
                float outW = maxW;
                float outH = outW / aspect;
                if (outH > maxH) {
                    outH = maxH;
                    outW = outH * aspect;
                }
                return ImVec2(outW, outH);
            };
            ImVec2 coverSize = coverSizeFor(texW, texH);

            if (ImGui::BeginTable("BookPopupLayout", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoHostExtendX)) {
                ImGui::TableSetupColumn("Cover", ImGuiTableColumnFlags_WidthFixed, coverSize.x + 36.0f);
                ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::BeginGroup();
                ImVec2 coverPos = ImGui::GetCursorScreenPos();
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 shadowA = ImVec2(coverPos.x + 6.0f, coverPos.y + 8.0f);
                ImVec2 shadowB = ImVec2(coverPos.x + coverSize.x + 10.0f, coverPos.y + coverSize.y + 12.0f);
                dl->AddRectFilled(shadowA, shadowB, IM_COL32(0, 0, 0, 70), 10.0f);

                if (coverSrv) {
                    ImGui::Image((ImTextureID)coverSrv, coverSize);
                }
                else {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.21f, 0.26f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
                    if (ImGui::BeginChild("CoverFallback", coverSize, true, ImGuiWindowFlags_NoScrollbar)) {
                        ImVec2 avail = ImGui::GetContentRegionAvail();
                        ImVec2 textSize = ImGui::CalcTextSize("No Cover");
                        ImVec2 pos = ImVec2((avail.x - textSize.x) * 0.5f, (avail.y - textSize.y) * 0.5f);
                        pos.x = (pos.x < 0.0f) ? 0.0f : pos.x;
                        pos.y = (pos.y < 0.0f) ? 0.0f : pos.y;
                        ImGui::SetCursorPos(pos);
                        ImGui::TextUnformatted("No Cover");
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                }

                ImGui::Dummy(ImVec2(0, 12));
                ImGui::EndGroup();

                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.97f, 1.0f, 1.0f));
                ImGui::TextWrapped("%s", it->title.c_str());
                ImGui::PopStyleColor();
                ImGui::TextDisabled("by %s", it->author.c_str());
                ImGui::TextDisabled("Category: %s", it->category.c_str());
                ImGui::Dummy(ImVec2(0, 4));

                ImVec4 statusCol;
                const char* statusText = nullptr;
                if (isBorrowedByUser) {
                    statusCol = ImVec4(0.75f, 0.85f, 1.0f, 1.0f);
                    statusText = "Borrowed by you";
                }
                else if (isBorrowedGlobally) {
                    statusCol = ImVec4(1.0f, 0.55f, 0.20f, 1.0f);
                    statusText = "Checked out";
                }
                else {
                    statusCol = ImVec4(0.20f, 0.88f, 0.45f, 1.0f);
                    statusText = "Available now";
                }
                ImGui::TextColored(statusCol, "%s", statusText);
                if (isBorrowedGlobally && !isBorrowedByUser) {
                    ImGui::TextDisabled("Check Borrowed History to see availability updates.");
                }

                ImGui::Separator();
                ImGui::TextWrapped("You can keep up to two active loans at a time. Borrowing now adds this book to your dashboard.");

                int userActive = 0;
                if (!currentUserStr.empty()) {
                    for (const auto& h : borrowHistory) {
                        if (h.borrowerName == currentUserStr && !h.isReturned) userActive++;
                    }
                }

                ImGui::Dummy(ImVec2(0, 6));
                float actionWidth = ImGui::GetContentRegionAvail().x;
                if (isBorrowedGlobally && !isBorrowedByUser) {
                    ImGui::BeginDisabled();
                    ImGui::Button("Unavailable", ImVec2(actionWidth, 0));
                    ImGui::EndDisabled();
                }
                else if (currentUserStr.empty()) {
                    ImGui::BeginDisabled();
                    ImGui::Button("Log in to borrow", ImVec2(actionWidth, 0));
                    ImGui::EndDisabled();
                }
                else if (userActive >= 2) {
                    ImGui::BeginDisabled();
                    ImGui::Button("Borrow limit reached", ImVec2(actionWidth, 0));
                    ImGui::EndDisabled();
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.1f, 1.0f), "Return a book to free a slot.");
                }
                else if (ImGui::Button("Borrow This Book", ImVec2(actionWidth, 0))) {
                    bool ok = SaveBorrowEntry(it->title, currentUsername);
                    if (ok) {
                        LoadBorrowHistory();
                        showBookDetailsPopup = false;
                    }
                    else {
                        ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1.0f), "Unable to borrow. Please try again.");
                    }
                }

                ImGui::Dummy(ImVec2(0, 6));
                if (currentUserStr.empty()) {
                    ImGui::BeginDisabled();
                    ImGui::Button("Log in to manage watchlist", ImVec2(actionWidth, 0));
                    ImGui::EndDisabled();
                }
                else {
                    const char* watchLabel = isFavorited ? "Remove From Watchlist" : "Add To Watchlist";
                    if (ImGui::Button(watchLabel, ImVec2(actionWidth, 0))) {
                        ToggleFavorite(currentUserStr, it->title);
                        isFavorited = !isFavorited;
                        watchCount = GetFavoriteCountForBook(it->title);
                    }
                }
                if (watchCount > 0) {
                    ImGui::TextDisabled("Watchers: %d", watchCount);
                }

                ImGui::EndTable();
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Close")) {
            showBookDetailsPopup = false;
        }

        ImGui::End();
    }
    ImGui::PopStyleVar(2);


    if (!showBookDetailsPopup) {
        selectedBookTitle = "";
    }
}


void RenderStudentDashboard(float fullSizeX, float fullSizeY, bool& loggedIn, const char* currentUsername) {
    static const char* currentCategory = "All";

    static std::map<std::string, char[64]> returnCodes;
    static std::string returnMessage = "";

    // Search buffers (new)
    static char searchAvailable[128] = "";
    static char searchBorrowed[128] = "";
    static std::string borrowMessage = ""; // shows feedback when trying to borrow

    const std::string currentUserStr = currentUsername ? currentUsername : "";
    const int overdueThresholdDays = 14;
    int studentBorrowCount = 0;
    int studentOverdueCount = 0;
    double totalUnpaidFines = 0.0;
    if (!currentUserStr.empty()) {
        for (const auto& entry : borrowHistory) {
            if (entry.borrowerName != currentUserStr || entry.isReturned) continue;
            studentBorrowCount++;
            int daysUntil = DaysUntilDue(entry.dueDate);
            if (daysUntil < 0) {
                studentOverdueCount++;
            }
        }
        totalUnpaidFines = GetTotalUnpaidFines(currentUserStr);
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(fullSizeX, fullSizeY));
    ImGui::Begin("Student Dashboard", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    float sidebarWidth = 300.0f;

    // Student sidebar now mirrors the admin composition for consistent branding
    ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth, fullSizeY), true);
    LogExpanded("BEGINCHILD", "Sidebar");
    RenderSidebarLogoHeader("FEU TECH", "E-Library Management System");

    float sidebarAvail = ImGui::GetContentRegionAvail().y;
    float studentProfileCardHeight = ImGui::GetTextLineHeightWithSpacing() * 6.5f + 40.0f;
    if (sidebarAvail > 0.0f) {
        studentProfileCardHeight = (std::min)(studentProfileCardHeight, sidebarAvail * 0.45f);
    }
    if (studentProfileCardHeight < 160.0f) studentProfileCardHeight = 160.0f;

    // Profile card mirrors admin layout but with student copy
    if (BeginSidebarCard("StudentProfileCard", studentProfileCardHeight, SIDEBAR_PROFILE_BG)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.74f, 0.86f, 1.0f, 1.0f));
        ImGui::TextUnformatted("Student");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Columns(2, "studentProfileColumns", false);
        auto infoRow = [&](const char* label, const std::string& value) {
            ImGui::TextDisabled("%s", label);
            ImGui::NextColumn();
            ImGui::TextWrapped("%s", value.c_str());
            ImGui::NextColumn();
        };
        infoRow("User", currentUsername ? currentUsername : "N/A");
        infoRow("Role", currentUser.role.empty() ? "Student" : currentUser.role);
        infoRow("Name", currentUser.name.empty() ? "N/A" : currentUser.name);
        infoRow("Section", currentUser.section.empty() ? "N/A" : currentUser.section);
        infoRow("Gender", currentUser.gender.empty() ? "N/A" : currentUser.gender);
        infoRow("Age", currentUser.age > 0 ? std::to_string(currentUser.age) : std::string("N/A"));
        ImGui::Columns(1);
    }
    EndSidebarCard();

    ImGui::Spacing();
    ImGui::Spacing();
    sidebarAvail = ImGui::GetContentRegionAvail().y;

    float studentActionHeight = ImGui::GetFrameHeightWithSpacing() * 3.0f + 50.0f;
    // Account actions stay identical to admin layout for familiarity
    if (BeginSidebarCard("StudentActionsCard", studentActionHeight, SIDEBAR_ACTION_BG)) {
        ImGui::TextUnformatted("Account Actions");
        ImGui::TextDisabled("Access profile tools");
        ImGui::Spacing();
        float actionWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("View Profile", ImVec2(actionWidth, 0))) {
            profileViewUser = UserProfile{};
            showProfilePopup = true;
        }
        ImGui::Spacing();
        if (ImGui::Button("Logout", ImVec2(actionWidth, 0))) {
            LogActivity(currentUser.username, "Logout", "User logged out");
            loggedIn = false;
            currentUser = UserProfile{};
            profileViewUser = UserProfile{};
        }
    }
    EndSidebarCard();

    ImGui::Spacing();

    sidebarAvail = ImGui::GetContentRegionAvail().y;
    float studentCategoryHeight = ImGui::GetTextLineHeightWithSpacing() * 6.0f + 140.0f;
    if (sidebarAvail > 0.0f) {
        studentCategoryHeight = (std::min)(studentCategoryHeight, sidebarAvail * 0.52f);
    }
    studentCategoryHeight = (std::max)(studentCategoryHeight, 240.0f);
    // Category filter lets students focus the catalog quickly
    if (BeginSidebarCard("StudentCategoryCard", studentCategoryHeight, SIDEBAR_CATEGORY_BG)) {
        ImGui::TextUnformatted("Categories");
        ImGui::TextDisabled("Browse by discipline");
        static char categorySearch[64] = "";
        ImGui::InputTextWithHint("##categorySearch", "Search categories...", categorySearch, IM_ARRAYSIZE(categorySearch));
        ImGui::Separator();

        float listHeight = ImGui::GetContentRegionAvail().y;
        if (listHeight < 140.0f) listHeight = 140.0f;
        std::string catSearchLow = categorySearch;
        std::transform(catSearchLow.begin(), catSearchLow.end(), catSearchLow.begin(), [](unsigned char c){ return (char)std::tolower(c); });

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.11f, 0.14f, 0.92f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        if (ImGui::BeginChild("StudentCategoryList", ImVec2(0, listHeight), true)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
            const int catColumns = 2;
            if (ImGui::BeginTable("StudentCategoryGrid", catColumns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH)) {
                for (auto& cat : categories) {
                    if (!catSearchLow.empty()) {
                        std::string catLow = cat;
                        std::transform(catLow.begin(), catLow.end(), catLow.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                        if (catLow.find(catSearchLow) == std::string::npos) continue;
                    }
                    ImGui::TableNextColumn();
                    bool selected = (currentCategory == cat);
                    ImVec4 base = ImVec4(0.18f, 0.18f, 0.20f, 0.60f);
                    ImVec4 accent = ImVec4(0.60f, 0.60f, 0.62f, 0.85f);
                    ImGui::PushStyleColor(ImGuiCol_Header, selected ? accent : base);
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(accent.x, accent.y, accent.z, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(accent.x, accent.y, accent.z, 1.0f));
                    if (ImGui::Selectable(cat.c_str(), selected, ImGuiSelectableFlags_SpanAvailWidth)) {
                        currentCategory = cat.c_str();
                    }
                    ImGui::PopStyleColor(3);
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
    EndSidebarCard();
    ImGui::Spacing();

    const float studentWatchHeight = 230.0f;
    // Watchlist mirrors admin styling for consistent cues
    if (BeginSidebarCard("StudentFavoritesCard", studentWatchHeight, SIDEBAR_WATCH_BG, ImVec2(18, 18))) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
        ImGui::TextUnformatted("Watchlist");
        ImGui::TextDisabled("Flag books to revisit later");
        ImGui::Separator();

        if (currentUserStr.empty()) {
            ImGui::TextDisabled("Login to start a watchlist.");
        } else {
            auto favorites = GetFavoritesForUser(currentUserStr);
            if (favorites.empty()) {
                ImGui::TextWrapped("Use the 'Add Watch' button on any book card to keep it here.");
            } else {
                for (const auto& title : favorites) {
                    ImGui::PushID(title.c_str());
                    ImGui::TextWrapped("%s", title.c_str());
                    const Book* matched = nullptr;
                    for (const auto& book : books) {
                        if (book.title == title) { matched = &book; break; }
                    }
                    if (matched) {
                        ImGui::TextDisabled("%s", matched->author.c_str());
                    }
                    bool availableNow = !IsBookBorrowed(title);
                    ImVec4 statusCol = availableNow ? ImVec4(0.19f, 0.84f, 0.42f, 1.0f) : ImVec4(0.96f, 0.63f, 0.30f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, statusCol);
                    ImGui::TextUnformatted(availableNow ? "Available" : "Borrowed");
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove##watch")) {
                        ToggleFavorite(currentUserStr, title);
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
            }
        }
    }
    EndSidebarCard();

    ImGui::Spacing();

    // Borrowed books card summarizes active loans + overdue count
    if (BeginSidebarCard("StudentBorrowedCard", 0.0f, SIDEBAR_BORROW_BG)) {
        ImGui::TextUnformatted("Your Borrowed Books");
        ImGui::TextDisabled("Return items or review due dates");
        ImGui::Spacing();

        if (ImGui::BeginTable("StudentBorrowStats", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.92f, 1.0f, 1.0f));
            ImGui::Text("%d", studentBorrowCount);
            ImGui::PopStyleColor();
            ImGui::TextDisabled("Active");

            ImGui::TableNextColumn();
            ImVec4 overdueCol = studentOverdueCount > 0 ? ImVec4(1.0f, 0.62f, 0.35f, 1.0f) : ImVec4(0.50f, 0.85f, 0.55f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, overdueCol);
            ImGui::Text("%d", studentOverdueCount);
            ImGui::PopStyleColor();
            ImGui::TextDisabled("Overdue");
            
            ImGui::TableNextColumn();
            ImVec4 fineCol = totalUnpaidFines > 0.0 ? ImVec4(1.0f, 0.35f, 0.25f, 1.0f) : ImVec4(0.50f, 0.85f, 0.55f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, fineCol);
            ImGui::Text("₱%.0f", totalUnpaidFines);
            ImGui::PopStyleColor();
            ImGui::TextDisabled("Fines");
            
            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::InputTextWithHint("##searchBorrowed", "Search your borrowed books (title)...", searchBorrowed, IM_ARRAYSIZE(searchBorrowed));
        ImGui::Spacing();

        float listHeight = ImGui::GetContentRegionAvail().y - 40.0f;
        if (listHeight < 220.0f) listHeight = 220.0f;

        if (!returnMessage.empty()) {
            ImGui::Spacing();
            ImGui::PushTextWrapPos(0);
            ImGui::TextColored(ImVec4(0.2f, 1, 0.2f, 1), "%s", returnMessage.c_str());
            ImGui::PopTextWrapPos();
            if (ImGui::Button("Clear Message", ImVec2(-1, 0))) returnMessage.clear();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.11f, 0.14f, 0.92f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::BeginChild("UserHistory", ImVec2(0, listHeight), true);
        LogExpanded("BEGINCHILD", "UserHistory");

        bool hasBorrowed = false;

        for (int i = static_cast<int>(borrowHistory.size()) - 1; i >= 0; --i) {
            const auto& entry = borrowHistory[i];
            if (entry.borrowerName != currentUserStr || entry.isReturned) continue;

            // Apply search filter if any
            if (searchBorrowed[0] != '\0') {
                std::string lowTitle = entry.bookTitle;
                std::string searchLow = searchBorrowed;
                std::transform(lowTitle.begin(), lowTitle.end(), lowTitle.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                std::transform(searchLow.begin(), searchLow.end(), searchLow.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                if (lowTitle.find(searchLow) == std::string::npos) {
                    continue;
                }
            }

            hasBorrowed = true;
            ImGui::PushID(i);
            LogExpanded("PUSHID", std::to_string(i).c_str());

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.19f, 0.24f, 0.88f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
            ImGuiWindowFlags cardFlags = ImGuiWindowFlags_NoScrollbar;
            ImGuiChildFlags childFlags = ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY;
            ImVec2 cardSize(ImGui::GetContentRegionAvail().x, 0.0f);
            if (ImGui::BeginChild(("BorrowCard_" + std::to_string(i)).c_str(), cardSize, childFlags, cardFlags)) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.97f, 1.0f, 1.0f));
                ImGui::TextWrapped("%s", entry.bookTitle.c_str());
                ImGui::PopStyleColor();
                ImGui::Separator();
                
                std::string elapsed = CalculateTimeElapsed(entry.borrowDate);
                ImGui::TextDisabled("Borrowed");
                ImGui::Text("  %s", entry.borrowDate.c_str());
                
                // Display due date
                if (!entry.dueDate.empty()) {
                    ImGui::TextDisabled("Due Date");
                    ImGui::Text("  %s", entry.dueDate.c_str());
                    
                    int daysUntil = DaysUntilDue(entry.dueDate);
                    ImGui::TextDisabled("Time Left");
                    if (daysUntil < 0) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.35f, 0.25f, 1.0f));
                        ImGui::Text("  OVERDUE by %d day(s)", -daysUntil);
                        ImGui::PopStyleColor();
                    } else if (daysUntil == 0) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.65f, 0.20f, 1.0f));
                        ImGui::Text("  DUE TODAY!");
                        ImGui::PopStyleColor();
                    } else if (daysUntil <= 3) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.80f, 0.35f, 1.0f));
                        ImGui::Text("  %d day(s) left", daysUntil);
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.85f, 0.55f, 1.0f));
                        ImGui::Text("  %d day(s) left", daysUntil);
                        ImGui::PopStyleColor();
                    }
                } else {
                    ImGui::TextDisabled("Elapsed");
                    ImGui::Text("  %s", elapsed.c_str());
                }
                
                // Display fine if overdue
                double currentFine = CalculateFine(entry.dueDate);
                if (currentFine > 0.0) {
                    ImGui::TextDisabled("Current Fine");
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.35f, 0.25f, 1.0f));
                    ImGui::Text("  ₱%.2f (@ ₱%.0f/day)", currentFine, FINE_PER_DAY);
                    ImGui::PopStyleColor();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (returnCodes.find(entry.bookTitle) == returnCodes.end()) {
                    std::fill(returnCodes[entry.bookTitle], returnCodes[entry.bookTitle] + 64, '\0');
                }
                std::string inputLabel = "Code##input_" + std::to_string(i);
                ImGui::PushItemWidth(-1);
                ImGui::InputTextWithHint(inputLabel.c_str(), "Enter return code", returnCodes[entry.bookTitle], 64, ImGuiInputTextFlags_CharsDecimal);
                ImGui::PopItemWidth();

                if (ImGui::Button("Return Book", ImVec2(ImGui::GetContentRegionAvail().x, 32.0f))) {
                    std::string codeInput = returnCodes[entry.bookTitle];
                    ReturnBook(entry.bookTitle, currentUserStr, codeInput);
                    LoadBorrowHistory();

                    bool returnedSuccessfully = true;
                    double finalFine = 0.0;
                    for (const auto& h_check : borrowHistory) {
                        if (h_check.bookTitle == entry.bookTitle && h_check.borrowerName == currentUserStr) {
                            if (!h_check.isReturned) {
                                returnedSuccessfully = false;
                            } else {
                                finalFine = h_check.fine;
                            }
                            break;
                        }
                    }

                    if (!returnedSuccessfully) {
                        returnMessage = "Error: Invalid return code for '" + entry.bookTitle + "'.";
                    }
                    else {
                        returnMessage = "Successfully returned '" + entry.bookTitle + "'.";
                        if (finalFine > 0.0) {
                            returnMessage += " Fine owed: ₱" + std::to_string((int)finalFine);
                        }
                        std::fill(returnCodes[entry.bookTitle], returnCodes[entry.bookTitle] + 64, '\0');
                    }
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
            LogExpanded("BEFORE_POPID", std::to_string(i).c_str());
            ImGui::PopID();
            LogExpanded("AFTER_POPID", std::to_string(i).c_str());
        }

        if (!hasBorrowed) {
            ImGui::TextDisabled("You have no books currently borrowed.");
        }

        LogExpanded("BEFORE_ENDCHILD", "UserHistory");
        EndChildSafe();
        LogExpanded("AFTER_ENDCHILD", "UserHistory");
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
    EndSidebarCard();
    ImGui::Spacing();

    EndChildSafe();

    // --- MAIN CONTENT (Available & Borrowed Books ) ---
    ImGui::SameLine();
    ImGui::BeginChild("BookGrid", ImVec2(fullSizeX - sidebarWidth - 20, fullSizeY), true);
    LogExpanded("BEGINCHILD", "BookGrid");

    int availableCount = 0;
    int borrowedCount = 0; // count of books borrowed by the current user (within selected category)

    std::unordered_set<std::string> borrowedAny;
    std::unordered_set<std::string> borrowedByCurrent;
    borrowedAny.reserve(borrowHistory.size());
    borrowedByCurrent.reserve(borrowHistory.size());
    for (const auto& h : borrowHistory) {
        if (h.isReturned) continue;
        borrowedAny.insert(h.bookTitle);
        if (!currentUserStr.empty() && h.borrowerName == currentUserStr) {
            borrowedByCurrent.insert(h.bookTitle);
        }
    }

    for (const auto& book : books) {
        if (std::string(currentCategory) != "All" && book.category != currentCategory) continue;
        if (!borrowedAny.count(book.title)) availableCount++;
        if (borrowedByCurrent.count(book.title)) borrowedCount++;
    }

    // Hero banner + stats row for a more polished library overview
    {
        std::string friendlyName = !currentUser.name.empty() ? currentUser.name : ((currentUsername && currentUsername[0]) ? currentUsername : "Student");
        std::string dateStr = GetFriendlyDateString();
        const float heroHeight = 150.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.19f, 0.96f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 18.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26, 20));
        if (ImGui::BeginChild("StudentHero", ImVec2(0, heroHeight), true, ImGuiWindowFlags_NoScrollbar)) {
            const float weights[] = { 3.0f, 1.2f };
            if (ImGui::BeginTable("HeroLayout", 2, ImGuiTableFlags_SizingStretchProp, ImVec2(0, heroHeight))) {
                ImGui::TableSetupColumn("HeroLeft", ImGuiTableColumnFlags_WidthStretch, weights[0]);
                ImGui::TableSetupColumn("HeroRight", ImGuiTableColumnFlags_WidthStretch, weights[1]);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, heroHeight - ImGui::GetStyle().ItemSpacing.y);

                // Left column
                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.92f, 1.0f, 1.0f));
                ImGui::PushFont(ImGui::GetFont());
                ImGui::SetWindowFontScale(1.18f);
                ImGui::Text("Welcome back, %s", friendlyName.c_str());
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopFont();
                ImGui::PopStyleColor();
                ImGui::TextDisabled("%s", dateStr.c_str());
                ImGui::Dummy(ImVec2(0, 6));
                ImGui::TextWrapped("Browsing the %s catalog. Use the filters below to discover and reserve titles instantly.", currentCategory);

                // Right column callout
                ImGui::TableSetColumnIndex(1);
                ImVec2 calloutAvail = ImGui::GetContentRegionAvail();
                float calloutWidth = (std::min)(calloutAvail.x, 210.0f);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (calloutAvail.x - calloutWidth));

                ImGui::BeginGroup();
                ImGui::Text("Today");
                ImGui::TextDisabled("%s", dateStr.c_str());
                ImGui::Dummy(ImVec2(0, 6));
                ImGui::Text("Happy reading!");
                ImGui::EndGroup();

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    {
        float statsWidth = ImGui::GetContentRegionAvail().x;
        const float spacing = 16.0f;
        float cardWidth = (statsWidth - spacing * 2.0f) / 3.0f;
        cardWidth = (std::max)((std::min)(cardWidth, 260.0f), 170.0f);

        float totalCardsWidth = cardWidth * 3.0f + spacing * 2.0f;
        float offset = (statsWidth - totalCardsWidth) * 0.5f;
        if (offset > 0.0f) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
        } else {
            totalCardsWidth = statsWidth;
        }

        std::string availableValue = std::to_string(availableCount);
        std::string borrowedValue = std::to_string(borrowedCount);
        std::string globalBorrowedValue = std::to_string(borrowedAny.size());
        std::string categoryDetail = std::string("Category: ") + currentCategory;
        std::string borrowedDetail = borrowedCount > 0 ? "Remember to return on time." : "No books borrowed yet.";
        std::string globalDetail = borrowedAny.empty() ? "All books are available." : "People who borrowed.";

        ImGui::BeginGroup();
        RenderStatCard("student_stat_available", "Available Now", availableValue, ImVec4(0.20f, 0.88f, 0.45f, 1.0f), cardWidth, categoryDetail.c_str());
        ImGui::SameLine(0.0f, spacing);
        RenderStatCard("student_stat_mine", "Your Books", borrowedValue, ImVec4(0.93f, 0.70f, 0.40f, 1.0f), cardWidth, borrowedDetail.c_str());
        ImGui::SameLine(0.0f, spacing);
        RenderStatCard("student_stat_global", "Borrowed (All)", globalBorrowedValue, ImVec4(0.92f, 0.50f, 0.36f, 1.0f), cardWidth, globalDetail.c_str());
        ImGui::EndGroup();
    }

    ImGui::Spacing();
    ImGui::InputTextWithHint("##searchAvailable", "(filter) Search available books (title)...", searchAvailable, IM_ARRAYSIZE(searchAvailable));
    ImGui::Separator();

    // Draw a subtle, centered background logo behind the UI (semi-transparent)
    {
        ImGuiIO& ioLocal = ImGui::GetIO();
        ID3D11ShaderResourceView* logoSrv = nullptr;
        int lw = 0, lh = 0;

        if (g_bookTextures.count("bglogo")) {
            logoSrv = g_bookTextures["bglogo"];
        } else {
            // attempt to load relative to resolved basePath (db/bglogo/bglogo.png)
            std::string logoPath = basePath + "bglogo\\bglogo.png";
            if (LoadTextureFromFile(logoPath.c_str(), &logoSrv, &lw, &lh)) {
                g_bookTextures["bglogo"] = logoSrv;
            }
        }

        if (logoSrv) {
            // Prefer stored size (set when we loaded the texture earlier). Fallback to local lw/lh or a safe default.
            if (g_bookTextureSizes.count("bglogo")) {
                auto p = g_bookTextureSizes["bglogo"];
                lw = p.first; lh = p.second;
            }
            if (lw == 0 || lh == 0) { lw = 1024; lh = 512; }

            ImDrawList* bg = ImGui::GetBackgroundDrawList();
            float screenW = ioLocal.DisplaySize.x;
            float screenH = ioLocal.DisplaySize.y;
            // Limit logo to 40% of screen to keep it subtle
            float maxW = screenW * 0.4f;
            float maxH = screenH * 0.4f;
            float imgW = (float)lw;
            float imgH = (float)lh;
            float scale1 = maxW / imgW;
            float scale2 = maxH / imgH;
            float scale = (scale1 < scale2) ? scale1 : scale2;
            float drawW = imgW * scale;
            float drawH = imgH * scale;
            // Center the logo on-screen (both horizontally and vertically)
            ImVec2 center = ImVec2(screenW * 0.5f, screenH * 0.5f);
            ImVec2 a = ImVec2(center.x - drawW * 0.5f, center.y - drawH * 0.5f);
            ImVec2 b = ImVec2(center.x + drawW * 0.5f, center.y + drawH * 0.5f);
            // Prevent upscaling (no stretching) — only downscale if needed
            if (scale > 1.0f) scale = 1.0f;
            // Draw with moderate alpha so logo is visible but subtle (140/255)
            bg->AddImage((ImTextureID)logoSrv, a, b, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 140));
        }
    }

    ImGui::BeginChild("HorizontalScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    LogExpanded("BEGINCHILD", "HorizontalScroll");

    // Responsive grid using ImGui table so items wrap into rows instead of one long SameLine row
    float avail = ImGui::GetContentRegionAvail().x;
    const float itemWidth = 200.0f; // approximate per-item width including padding (wider for better spacing)
    int columns = (std::max)(1, (int)(avail / itemWidth));

    if (ImGui::BeginTable("books_table", columns, ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {
        LogExpanded("BEGINTABLE", "books_table");
        for (auto& book : books) {
            if (std::string(currentCategory) != "All" && book.category != currentCategory) continue;

            // Apply library-wide search filter (title/author/category) if provided
            if (strlen(searchLibraryGlobal) > 0) {
                std::string combined = book.title + " " + book.author + " " + book.category;
                std::string combinedLow = combined;
                std::string searchLow = searchLibraryGlobal;
                std::transform(combinedLow.begin(), combinedLow.end(), combinedLow.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                std::transform(searchLow.begin(), searchLow.end(), searchLow.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                if (combinedLow.find(searchLow) == std::string::npos) {
                    continue; // skip non-matching
                }
            }

            // Apply available-only search filter (match title) as secondary filter
            if (strlen(searchAvailable) > 0) {
                std::string lowTitle = book.title;
                std::string searchLow = searchAvailable;
                std::transform(lowTitle.begin(), lowTitle.end(), lowTitle.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                std::transform(searchLow.begin(), searchLow.end(), searchLow.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                if (lowTitle.find(searchLow) == std::string::npos) {
                    continue; // skip non-matching
                }
            }

            ImGui::TableNextColumn();
            ImGui::PushID(book.title.c_str());
            LogExpanded("PUSHID", book.title.c_str());
            ImGui::BeginGroup();

            bool isUserBorrowed = borrowedByCurrent.count(book.title) > 0;
            bool isBorrowedGlobal = borrowedAny.count(book.title) > 0;
            bool isFavorited = IsBookFavorited(currentUserStr, book.title);
            int watchCount = GetFavoriteCountForBook(book.title);

            // Unified book card renderer with hover-scale animation, overlay and shadow
            static std::map<std::string, float> coverScales;
            const float baseW = 140.0f; // larger base width for covers
            const float maxH = 200.0f;  // allow taller covers for better aspect preservation

            // Load texture if needed
            ID3D11ShaderResourceView* srv = nullptr;
            int texW = 0, texH = 0;
            if (!book.iconPath.empty()) {
                if (g_bookTextures.count(book.iconPath)) {
                    srv = g_bookTextures[book.iconPath];
                    if (g_bookTextureSizes.count(book.iconPath)) {
                        auto p = g_bookTextureSizes[book.iconPath]; texW = p.first; texH = p.second;
                    }
                } else {
                    if (LoadTextureFromFile(book.iconPath.c_str(), &srv, &texW, &texH)) {
                        g_bookTextures[book.iconPath] = srv;
                        g_bookTextureSizes[book.iconPath] = { texW, texH };
                    } else srv = nullptr;
                }
            }

            // Determine draw size keeping aspect ratio
            float drawW = baseW;
            float drawH = maxH;
            if (srv && texW > 0 && texH > 0) {
                float aspect = (float)texW / (float)texH;
                drawW = baseW;
                drawH = drawW / aspect;
                if (drawH > maxH) {
                    drawH = maxH;
                    drawW = drawH * aspect;
                }
            } else {
                // placeholder aspect (3:4)
                drawW = baseW;
                drawH = baseW * 1.33f;
                if (drawH > maxH) drawH = maxH;
            }

            // Animated scale (stored per-book)
            float& scale = coverScales[book.title];
            if (scale <= 0.001f) scale = 1.0f; // initialize

            // Draw a subtle shadow behind the cover
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImVec2 shadowA = ImVec2(cursor.x + 4.0f, cursor.y + 6.0f);
            ImVec2 shadowB = ImVec2(cursor.x + drawW + 8.0f, cursor.y + drawH + 10.0f);
            dl->AddRectFilled(shadowA, shadowB, IM_COL32(0,0,0,60), 6.0f);

            // Reserve space and draw image (apply current scale)
            ImVec2 availCursor = ImGui::GetCursorPos();
            ImGui::Dummy(ImVec2(0,0)); // no-op to keep layout predictable
            ImGui::SetCursorScreenPos(cursor);
            ImVec2 imgSize = ImVec2(drawW * scale, drawH * scale);

            if (srv) {
                ImGui::Image((ImTextureID)srv, imgSize);
                if (ImGui::IsItemClicked()) {
                    selectedBookTitle = book.title;
                    showBookDetailsPopup = true;
                }
            } else {
                // Fallback: draw an icon button (use Font Awesome font when loaded)
                const char* ICON_BOOK = g_fontAwesomeLoaded ? u8"\uF02D" : "[BOOK]";
                const char* ICON_LOCK = g_fontAwesomeLoaded ? u8"\uF023" : "[LOCK]";
                if (isBorrowedGlobal) {
                    if (g_fontAwesomeLoaded && g_fontAwesome) { ImGui::PushFont(g_fontAwesome); if (ImGui::Button(ICON_LOCK, imgSize)) { selectedBookTitle = book.title; showBookDetailsPopup = true; } ImGui::PopFont(); }
                    else { if (ImGui::Button(ICON_LOCK, imgSize)) { selectedBookTitle = book.title; showBookDetailsPopup = true; } }
                } else {
                    if (g_fontAwesomeLoaded && g_fontAwesome) { ImGui::PushFont(g_fontAwesome); if (ImGui::Button(ICON_BOOK, imgSize)) { selectedBookTitle = book.title; showBookDetailsPopup = true; } ImGui::PopFont(); }
                    else { if (ImGui::Button(ICON_BOOK, imgSize)) { selectedBookTitle = book.title; showBookDetailsPopup = true; } }
                }
            }

            // After rendering, detect hover and animate towards target scale
            bool hovered = ImGui::IsItemHovered();
            float target = hovered ? 1.06f : 1.0f;
            float dt = ImGui::GetIO().DeltaTime;
            float speed = 10.0f; // responsiveness of interpolation
            float alpha = ((dt * speed) < 1.0f ? (dt * speed) : 1.0f);
            scale = scale + (target - scale) * alpha;

            // Title and author shown below the cover (better readability and avoids overlay)
            ImVec2 a = ImGui::GetItemRectMin();
            ImVec2 b = ImGui::GetItemRectMax();
            // Use image width as maximum text width
            float overlayMaxW = imgSize.x;
            auto ellipsize = [&](const std::string& s, float maxW) {
                if (ImGui::CalcTextSize(s.c_str()).x <= maxW) return s;
                std::string t = s;
                while (!t.empty() && ImGui::CalcTextSize((t + "...").c_str()).x > maxW) t.pop_back();
                return t + "...";
            };
            std::string titleShort = ellipsize(book.title, overlayMaxW);
            // Determine a simple status string (user-friendly capitalization)
            std::string status;
            if (isUserBorrowed) status = "Borrowed";
            else if (isBorrowedGlobal) status = "Unavailable";
            else status = "Available";

            // Small spacing between cover and text area
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);

            // Status line: green when available, orange when not available
            bool available = !isBorrowedGlobal;
            ImVec4 statusCol = available ? ImVec4(0.20f, 0.90f, 0.40f, 1.0f) : ImVec4(1.00f, 0.60f, 0.15f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, statusCol);
            ImGui::TextUnformatted(status.c_str());
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0, 2));
            const char* watchLabel = isFavorited ? "Remove Watch" : "Add Watch";
            if (currentUserStr.empty()) {
                ImGui::BeginDisabled();
                ImGui::SmallButton("Add Watch");
                ImGui::EndDisabled();
            } else if (ImGui::SmallButton(watchLabel)) {
                ToggleFavorite(currentUserStr, book.title);
                isFavorited = !isFavorited;
                watchCount = GetFavoriteCountForBook(book.title);
            }

            if (isFavorited) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.82f, 0.30f, 1.0f));
                ImGui::TextUnformatted("On watchlist");
                ImGui::PopStyleColor();
            }
            if (watchCount > 0) {
                ImGui::TextDisabled("Watchers: %d", watchCount);
            }

            // Title line: primary, brighter and slightly larger-looking
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::TextUnformatted(titleShort.c_str());
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0,6));
            ImGui::EndGroup();
            LogExpanded("BEFORE_POPID", book.title.c_str());
            ImGui::PopID();
            LogExpanded("AFTER_POPID", book.title.c_str());
        }
        LogExpanded("BEFORE_ENDTABLE", "books_table");
        ImGui::EndTable();
        LogExpanded("AFTER_ENDTABLE", "books_table");
    }

    LogExpanded("BEFORE_ENDCHILD", "HorizontalScroll");
    EndChildSafe();
    LogExpanded("AFTER_ENDCHILD", "HorizontalScroll");

    LogExpanded("BEFORE_ENDCHILD", "BookGrid");
    EndChildSafe();
    LogExpanded("AFTER_ENDCHILD", "BookGrid");

    ImGui::End();

    // Render popups only if active
}


void RenderAdminDashboard(float fullSizeX, float fullSizeY, bool& loggedIn, const char* currentUsername) {
    static const char* currentCategory = "All";
    static int adminTab = 0;
    enum class ImportPreviewSource { None, Catalog, Manage };
    static ImportPreviewSource pendingImportSource = ImportPreviewSource::None;
    static bool showImportPreview = false;
    static std::string pendingImportPath;
    static BookDiffResult pendingDiffResult;
    static std::string manageBooksStatusMessage;
    static std::string catalogImportMessage;
    static char adminCategorySearch[64] = "";
    const std::string adminUserStr = currentUsername ? currentUsername : "";
    const int adminOverdueThresholdDays = 14;
    int adminBorrowCount = 0;
    int adminOverdueCount = 0;
    if (!adminUserStr.empty()) {
        for (const auto& entry : borrowHistory) {
            if (entry.borrowerName != adminUserStr || entry.isReturned) continue;
            adminBorrowCount++;
            if (DaysSinceBorrow(entry.borrowDate) >= adminOverdueThresholdDays) {
                adminOverdueCount++;
            }
        }
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(fullSizeX, fullSizeY));
    ImGui::Begin("Admin Dashboard", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    DebugLogIDStack("ADMIN_ENTER");
    float sidebarWidth = 300.0f;

    // Admin sidebar mirrors student layout but exposes moderation tools
    ImGui::BeginChild("AdminSidebar", ImVec2(sidebarWidth, fullSizeY), true);
    DebugLogIDStack("ADMIN_BEGINCHILD AdminSidebar");
    RenderSidebarLogoHeader("FEU TECH", "E-Library Management System");

    float adminSidebarAvail = ImGui::GetContentRegionAvail().y;
    float adminProfileHeight = ImGui::GetTextLineHeightWithSpacing() * 6.5f + 40.0f;
    if (adminSidebarAvail > 0.0f) {
        adminProfileHeight = (std::min)(adminProfileHeight, adminSidebarAvail * 0.45f);
    }
    if (adminProfileHeight < 160.0f) adminProfileHeight = 160.0f;

    if (BeginSidebarCard("AdminProfileCard", adminProfileHeight, SIDEBAR_PROFILE_BG)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.90f, 1.0f, 1.0f));
        ImGui::TextUnformatted("Administrator");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Columns(2, "adminProfileColumns", false);
        auto infoRow = [&](const char* label, const std::string& value) {
            ImGui::TextDisabled("%s", label);
            ImGui::NextColumn();
            ImGui::TextWrapped("%s", value.c_str());
            ImGui::NextColumn();
        };
        infoRow("User", currentUsername ? currentUsername : "N/A");
        infoRow("Role", currentUser.role.empty() ? "admin" : currentUser.role);
        infoRow("Name", currentUser.name.empty() ? "N/A" : currentUser.name);
        infoRow("Section", currentUser.section.empty() ? "N/A" : currentUser.section);
        infoRow("Gender", currentUser.gender.empty() ? "N/A" : currentUser.gender);
        infoRow("Age", currentUser.age > 0 ? std::to_string(currentUser.age) : std::string("N/A"));
        ImGui::Columns(1);
    }
    EndSidebarCard();

    ImGui::Spacing();
    adminSidebarAvail = ImGui::GetContentRegionAvail().y;

    float adminActionHeight = ImGui::GetFrameHeightWithSpacing() * 4.5f + 50.0f;
    // Ensure profile + logout remain always reachable for admins
    if (BeginSidebarCard("AdminActionsCard", adminActionHeight, SIDEBAR_ACTION_BG)) {
        ImGui::TextUnformatted("Account Actions");
        ImGui::TextDisabled("Access profile tools");
        ImGui::Spacing();
        float actionWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("View Profile", ImVec2(actionWidth, 0))) {
            profileViewUser = currentUser;
            showProfilePopup = true;
        }
        ImGui::Spacing();
        if (ImGui::Button("Fine Settings", ImVec2(actionWidth, 0))) {
            tempLoanPeriod = LOAN_PERIOD_DAYS;
            tempFinePerDay = (float)FINE_PER_DAY;
            tempMaxFine = (float)MAX_FINE_AMOUNT;
            settingsMessage.clear();
            showSettingsPopup = true;
        }
        ImGui::Spacing();
        if (ImGui::Button("Logout", ImVec2(actionWidth, 0))) {
            LogActivity(currentUser.username, "Logout", "User logged out");
            loggedIn = false;
            currentUser = UserProfile{};
            profileViewUser = UserProfile{};
        }
    }
    EndSidebarCard();

    adminSidebarAvail = ImGui::GetContentRegionAvail().y;
    float adminCategoryHeight = ImGui::GetTextLineHeightWithSpacing() * 6.0f + 140.0f;
    if (adminSidebarAvail > 0.0f) {
        adminCategoryHeight = (std::min)(adminCategoryHeight, adminSidebarAvail * 0.52f);
    }
    adminCategoryHeight = (std::max)(adminCategoryHeight, 240.0f);
    // Categories block keeps the catalog context aligned for edits
    if (BeginSidebarCard("AdminCategoryCard", adminCategoryHeight, SIDEBAR_CATEGORY_BG)) {
        ImGui::TextUnformatted("Categories");
        ImGui::TextDisabled("Focus the catalog view");
        ImGui::Spacing();
        ImGui::InputTextWithHint("##admin_category_search", "Search categories...", adminCategorySearch, IM_ARRAYSIZE(adminCategorySearch));
        ImGui::Separator();
        std::string catSearchLow = adminCategorySearch;
        std::transform(catSearchLow.begin(), catSearchLow.end(), catSearchLow.begin(), [](unsigned char c){ return (char)std::tolower(c); });

        float adminListHeight = ImGui::GetContentRegionAvail().y;
        if (adminListHeight < 160.0f) adminListHeight = 160.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.10f, 0.14f, 0.92f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        if (ImGui::BeginChild("AdminCategoryList", ImVec2(0, adminListHeight), true)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
            const int adminCatColumns = 2;
            if (ImGui::BeginTable("AdminCategoryGrid", adminCatColumns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH)) {
                for (auto& cat : categories) {
                    if (!catSearchLow.empty()) {
                        std::string catLow = cat;
                        std::transform(catLow.begin(), catLow.end(), catLow.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                        if (catLow.find(catSearchLow) == std::string::npos) continue;
                    }
                    ImGui::TableNextColumn();
                    bool selected = (currentCategory == cat);
                    ImVec4 base = ImVec4(0.18f, 0.18f, 0.20f, 0.60f);
                    ImVec4 accent = ImVec4(0.65f, 0.65f, 0.66f, 0.85f);
                    ImGui::PushStyleColor(ImGuiCol_Header, selected ? accent : base);
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(accent.x, accent.y, accent.z, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(accent.x, accent.y, accent.z, 1.0f));
                    if (ImGui::Selectable(cat.c_str(), selected, ImGuiSelectableFlags_SpanAvailWidth)) {
                        currentCategory = cat.c_str();
                    }
                    ImGui::PopStyleColor(3);
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
    EndSidebarCard();

    ImGui::Spacing();
    const float adminWatchHeight = 230.0f;
    // Admin watchlist uses the same favorite store for parity
    if (BeginSidebarCard("AdminWatchlistCard", adminWatchHeight, SIDEBAR_WATCH_BG, ImVec2(18, 18))) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
        ImGui::TextUnformatted("Watchlist");
        ImGui::TextDisabled("Flag books to revisit later");
        ImGui::Separator();
        if (adminUserStr.empty()) {
            ImGui::TextDisabled("Sign in to manage a watchlist.");
        } else {
            auto adminFavorites = GetFavoritesForUser(adminUserStr);
            if (adminFavorites.empty()) {
                ImGui::TextWrapped("Use the 'Add Watch' button on any book card to pin it here.");
            } else {
                for (const auto& title : adminFavorites) {
                    ImGui::PushID(title.c_str());
                    ImGui::TextWrapped("%s", title.c_str());
                    const Book* matched = nullptr;
                    for (const auto& book : books) {
                        if (book.title == title) { matched = &book; break; }
                    }
                    if (matched) {
                        ImGui::TextDisabled("%s", matched->author.c_str());
                    }
                    bool availableNow = !IsBookBorrowed(title);
                    ImVec4 statusCol = availableNow ? ImVec4(0.18f, 0.86f, 0.45f, 1.0f) : ImVec4(0.96f, 0.63f, 0.30f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, statusCol);
                    ImGui::TextUnformatted(availableNow ? "Available" : "Borrowed");
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove##admin_watch")) {
                        ToggleFavorite(adminUserStr, title);
                        ImGui::PopID();
                        continue;
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
            }
        }
    }
    EndSidebarCard();

    // --- Admin: Show current admin's borrowed books and count (parity with Student view)
    ImGui::Spacing();
    static char adminSearchBorrowed[128] = "";
    // Admin borrowed view shows their own loans for transparency
    if (BeginSidebarCard("AdminBorrowedCard", 0.0f, SIDEBAR_BORROW_BG)) {
        ImGui::TextUnformatted("Your Borrowed Books");
        ImGui::TextDisabled("Return or review your loans");
        ImGui::Spacing();

        if (ImGui::BeginTable("AdminBorrowStats", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.92f, 1.0f, 1.0f));
            ImGui::Text("%d", adminBorrowCount);
            ImGui::PopStyleColor();
            ImGui::TextDisabled("Active Loans");

            ImGui::TableNextColumn();
            ImVec4 overdueCol = adminOverdueCount > 0 ? ImVec4(1.0f, 0.62f, 0.35f, 1.0f) : ImVec4(0.50f, 0.85f, 0.55f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, overdueCol);
            ImGui::Text("%d", adminOverdueCount);
            ImGui::PopStyleColor();
            ImGui::TextDisabled("Due Soon");
            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::InputTextWithHint("##admin_searchBorrowed", "Search your borrowed books (title)...", adminSearchBorrowed, IM_ARRAYSIZE(adminSearchBorrowed));
        ImGui::Spacing();

        float adminListHeight = ImGui::GetContentRegionAvail().y - 40.0f;
        if (adminListHeight < 220.0f) adminListHeight = 220.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.11f, 0.14f, 0.9f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        if (ImGui::BeginChild("AdminUserHistory", ImVec2(0, adminListHeight), true)) {
            bool adminHasBorrowed = false;
            for (int i = (int)borrowHistory.size() - 1; i >= 0; --i) {
                if (borrowHistory[i].borrowerName != adminUserStr) continue;
                if (borrowHistory[i].isReturned) continue;

                if (strlen(adminSearchBorrowed) > 0) {
                    std::string lowTitle = borrowHistory[i].bookTitle;
                    std::string searchLow = adminSearchBorrowed;
                    std::transform(lowTitle.begin(), lowTitle.end(), lowTitle.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                    std::transform(searchLow.begin(), searchLow.end(), searchLow.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                    if (lowTitle.find(searchLow) == std::string::npos) continue;
                }

                adminHasBorrowed = true;
                ImGui::TextWrapped("%s", borrowHistory[i].bookTitle.c_str());
                ImGui::TextDisabled("Borrowed: %s", borrowHistory[i].borrowDate.c_str());
                // allow quick mark returned from sidebar for convenience
                if (ImGui::SmallButton((std::string("Mark Returned##adm") + std::to_string(i)).c_str())) {
                    AdminMarkEntryReturned(i);
                }
                ImGui::Separator();
            }
            if (!adminHasBorrowed) {
                ImGui::TextDisabled("You have no books currently borrowed.");
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
    EndSidebarCard();

    ImGui::Spacing();
    static char adminGlobalSearch[128] = "";
    // Global borrowed list enables forced returns / copy codes
    if (BeginSidebarCard("AdminBorrowedGlobalCard", 0.0f, SIDEBAR_GLOBAL_BG)) {
        ImGui::TextUnformatted("Borrowed History (All Users)");
        ImGui::TextDisabled("Track outstanding loans");
        ImGui::Spacing();
        ImGui::InputTextWithHint("##admin_global_search", "Search book or borrower...", adminGlobalSearch, IM_ARRAYSIZE(adminGlobalSearch));
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.11f, 0.14f, 0.9f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("AdminGlobalBorrowed", ImVec2(0, 260), true);
    bool adminHasGlobal = false;
    std::string globalSearch = adminGlobalSearch;
    std::transform(globalSearch.begin(), globalSearch.end(), globalSearch.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    for (int i = (int)borrowHistory.size() - 1; i >= 0; --i) {
        HistoryEntry &entry = borrowHistory[i];
        if (entry.isReturned) continue;

        if (!globalSearch.empty()) {
            std::string hay = entry.bookTitle + " " + entry.borrowerName;
            std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c){ return (char)std::tolower(c); });
            if (hay.find(globalSearch) == std::string::npos) continue;
        }

        adminHasGlobal = true;
        ImGui::PushID(i + 4000);
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s", entry.bookTitle.c_str());
        ImGui::TextDisabled("Borrower: %s", entry.borrowerName.c_str());
        ImGui::TextDisabled("Borrowed: %s", entry.borrowDate.c_str());
        ImGui::TextDisabled("Elapsed: %s", CalculateTimeElapsed(entry.borrowDate).c_str());

        ImGui::Spacing();
        const float adminEntryButtonHeight = 26.0f;
        const float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
        int buttonCount = entry.returnCode.empty() ? 1 : 2;
        float availWidth = ImGui::GetContentRegionAvail().x;
        float rawWidth = (availWidth - buttonSpacing * (buttonCount - 1)) / buttonCount;
        float adminActionWidth = (std::max)(80.0f, (std::min)(rawWidth, 150.0f));
        float totalWidth = adminActionWidth * buttonCount + buttonSpacing * (buttonCount - 1);
        float cursorX = ImGui::GetCursorPosX();
        float startOffset = (availWidth - totalWidth) * 0.5f;
        if (startOffset > 0.0f) ImGui::SetCursorPosX(cursorX + startOffset);

        if (!entry.returnCode.empty()) {
            if (ImGui::Button("Copy Code", ImVec2(adminActionWidth, adminEntryButtonHeight))) {
                ImGui::SetClipboardText(entry.returnCode.c_str());
            }
            ImGui::SameLine(0.0f, buttonSpacing);
        }

        if (ImGui::Button((std::string("Force Return##") + std::to_string(i)).c_str(), ImVec2(adminActionWidth, adminEntryButtonHeight))) {
            AdminMarkEntryReturned(i);
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    if (!adminHasGlobal) {
        ImGui::TextDisabled("No outstanding borrows.");
    }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
    EndSidebarCard();

    DebugLogIDStack("ADMIN_BEFORE_ENDCHILD AdminSidebar");
    EndChildSafe();
    DebugLogIDStack("ADMIN_AFTER_ENDCHILD AdminSidebar");

    // Main Content Area
    ImGui::SameLine();
    ImGui::BeginChild("AdminMainContent", ImVec2(fullSizeX - sidebarWidth - 20, fullSizeY), true);
    LogExpanded("BEGINCHILD", "AdminMainContent");

    // Draw the same centered, non-stretched bglogo for Admin view as well
    {
        ImGuiIO& ioLocalAdmin = ImGui::GetIO();
        ID3D11ShaderResourceView* logoSrvAdmin = nullptr;
        int lw_admin = 0, lh_admin = 0;

        if (g_bookTextures.count("bglogo")) {
            logoSrvAdmin = g_bookTextures["bglogo"];
        }
        if (logoSrvAdmin) {
            if (g_bookTextureSizes.count("bglogo")) {
                auto p = g_bookTextureSizes["bglogo"];
                lw_admin = p.first; lh_admin = p.second;
            }
            if (lw_admin == 0 || lh_admin == 0) { lw_admin = 1024; lh_admin = 512; }

            ImDrawList* bgAdmin = ImGui::GetBackgroundDrawList();
            float screenW = ioLocalAdmin.DisplaySize.x;
            float screenH = ioLocalAdmin.DisplaySize.y;
            float maxW = screenW * 0.4f;
            float maxH = screenH * 0.4f;
            float imgW = (float)lw_admin;
            float imgH = (float)lh_admin;
            float scale1 = maxW / imgW;
            float scale2 = maxH / imgH;
            float scale = (scale1 < scale2) ? scale1 : scale2;
            if (scale > 1.0f) scale = 1.0f;
            float drawW = imgW * scale;
            float drawH = imgH * scale;
            ImVec2 center = ImVec2(screenW * 0.5f, screenH * 0.5f);
            ImVec2 a = ImVec2(center.x - drawW * 0.5f, center.y - drawH * 0.5f);
            ImVec2 b = ImVec2(center.x + drawW * 0.5f, center.y + drawH * 0.5f);
            bgAdmin->AddImage((ImTextureID)logoSrvAdmin, a, b, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 140));
        }
    }

    // Hero banner + stats row before tab content
    {
        std::string friendlyName = !currentUser.name.empty() ? currentUser.name : ((currentUsername && currentUsername[0]) ? currentUsername : "Admin");
        std::string dateStr = GetFriendlyDateString();
        const float heroHeight = 130.0f;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.17f, 0.21f, 0.90f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22, 18));
        if (ImGui::BeginChild("AdminHero", ImVec2(0, heroHeight), true, ImGuiWindowFlags_NoScrollbar)) {
            if (ImGui::BeginTable("AdminHeroLayout", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("HeroInfo", ImGuiTableColumnFlags_WidthStretch, 3.0f);
                ImGui::TableSetupColumn("HeroActions", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, heroHeight - ImGui::GetStyle().ItemSpacing.y);

                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.90f, 1.0f, 1.0f));
                ImGui::Text("Hello, %s", friendlyName.c_str());
                ImGui::PopStyleColor();
                ImGui::TextDisabled("%s", dateStr.c_str());
                ImGui::Spacing();
                ImGui::TextWrapped("Stay on top of catalog updates, approvals, and returns in real time.");

                ImGui::TableSetColumnIndex(1);
                ImVec2 buttonSize(150.0f, 34.0f);
                ImVec2 actionAvail = ImGui::GetContentRegionAvail();
                ImVec2 buttonPos = ImGui::GetCursorPos();
                if (actionAvail.x > buttonSize.x) buttonPos.x += (actionAvail.x - buttonSize.x) * 0.5f;
                if (actionAvail.y > buttonSize.y) buttonPos.y += (actionAvail.y - buttonSize.y) * 0.5f;
                ImGui::SetCursorPos(buttonPos);
                if (ImGui::Button("Refresh Data", buttonSize)) {
                    LoadBooksFromCSV();
                    LoadBorrowHistory();
                    LoadUsersFromCSV();
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }

    std::unordered_set<std::string> adminBorrowedTitles;
    adminBorrowedTitles.reserve(borrowHistory.size());
    for (const auto& entry : borrowHistory) {
        if (entry.isReturned) continue;
        adminBorrowedTitles.insert(entry.bookTitle);
    }
    int totalBooks = (int)books.size();
    int borrowedNow = (int)adminBorrowedTitles.size();
    int availableNow = (std::max)(0, totalBooks - borrowedNow);
    int totalUsers = (int)users.size();
    int adminCount = 0;
    for (const auto& kv : users) if (kv.second.role == "admin") adminCount++;
    int studentCount = (std::max)(0, totalUsers - adminCount);
    int categoriesCount = (int)categories.size();
    int percentAvailable = (totalBooks > 0) ? (availableNow * 100) / totalBooks : 0;

    ImGui::Spacing();
    {
        float statsWidth = ImGui::GetContentRegionAvail().x;
        float cardWidth = (statsWidth - 30.0f) / 4.0f;
        if (cardWidth < 140.0f) cardWidth = 140.0f;
        std::string totalBooksStr = std::to_string(totalBooks);
        std::string totalDetail = std::to_string(categoriesCount) + " categories";
        std::string borrowedStr = std::to_string(borrowedNow);
        std::string borrowedDetail = borrowedNow > 0 ? "Awaiting returns" : "All books back";
        std::string availableStr = std::to_string(availableNow);
        std::string availableDetail = std::to_string(percentAvailable) + "% ready";
        std::string userStr = std::to_string(totalUsers);
        std::string userDetail = std::to_string(studentCount) + " students / " + std::to_string(adminCount) + " admins";
        RenderStatCard("admin_stat_total", "Books in Catalog", totalBooksStr, ImVec4(0.28f, 0.62f, 0.98f, 1.0f), cardWidth, totalDetail.c_str());
        ImGui::SameLine(0.0f, 10.0f);
        RenderStatCard("admin_stat_borrowed", "Borrowed Now", borrowedStr, ImVec4(1.00f, 0.58f, 0.30f, 1.0f), cardWidth, borrowedDetail.c_str());
        ImGui::SameLine(0.0f, 10.0f);
        RenderStatCard("admin_stat_available", "Available", availableStr, ImVec4(0.28f, 0.86f, 0.52f, 1.0f), cardWidth, availableDetail.c_str());
        ImGui::SameLine(0.0f, 10.0f);
        RenderStatCard("admin_stat_users", "Active Users", userStr, ImVec4(0.90f, 0.64f, 1.0f, 1.0f), cardWidth, userDetail.c_str());
    }

    ImGui::Spacing();

    auto drawTabButton = [&](const char* label, int idx) {
        bool active = (adminTab == idx);
        ImVec4 base = active ? ImVec4(0.16f, 0.58f, 0.82f, 1.0f) : ImGui::GetStyle().Colors[ImGuiCol_Button];
        auto clamp = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
        ImVec4 hover = ImVec4(clamp(base.x + 0.08f), clamp(base.y + 0.08f), clamp(base.z + 0.08f), base.w);
        ImVec4 activeCol = ImVec4(clamp(base.x - 0.04f), clamp(base.y - 0.04f), clamp(base.z - 0.04f), base.w);
        ImGui::PushStyleColor(ImGuiCol_Button, base);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeCol);
        if (ImGui::Button(label)) {
            adminTab = idx;
        }
        ImGui::PopStyleColor(3);
    };

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 10));
    drawTabButton("Book Catalog", 0);
    ImGui::SameLine();
    drawTabButton("Borrow History", 1);
    ImGui::SameLine();
    drawTabButton("User Management", 2);
    ImGui::SameLine();
    drawTabButton("Manage Books", 3);
    ImGui::SameLine();
    drawTabButton("Activity Logs", 4);
    ImGui::PopStyleVar();

    ImGui::Separator();

    if (adminTab == 0) { // Book Catalog (Grid View) Admin Dashboard
        ImGui::Text(" Book Catalog - Category: %s", currentCategory);
        ImGui::Separator();
        ImGui::InputTextWithHint("##searchLibraryAdmin", "Search library (title, author, category)...", searchLibraryGlobal, IM_ARRAYSIZE(searchLibraryGlobal));

        // Library import/export controls (admin)
        static char libraryPath[512] = "";
        static bool first_lib_init = true;
        if (first_lib_init) {
            std::string defaultPath = booksCSV;
            strncpy_s(libraryPath, sizeof(libraryPath), defaultPath.c_str(), _TRUNCATE);
            libraryPath[sizeof(libraryPath) - 1] = '\0';
            first_lib_init = false;
        }

        // Admin manual add/import: remove export function and provide manual entry fields
        ImGui::InputText("##libraryPath", libraryPath, IM_ARRAYSIZE(libraryPath));
        ImGui::SameLine();
        if (ImGui::Button("Import Library (CSV)", ImVec2(160, 0))) {
            std::vector<Book> incoming;
            if (!LoadBooksFromCSVPath(libraryPath, incoming)) {
                catalogImportMessage = std::string("Import failed: unable to read ") + libraryPath;
                showImportPreview = false;
            } else {
                pendingDiffResult = ComputeBookDiff(books, incoming);
                pendingImportPath = libraryPath;
                pendingImportSource = ImportPreviewSource::Catalog;
                showImportPreview = true;
                ImGui::OpenPopup("CSV Import Preview");
                catalogImportMessage = std::string("Previewing changes from: ") + libraryPath;
            }
        }
        if (!catalogImportMessage.empty()) {
            ImGui::TextWrapped("%s", catalogImportMessage.c_str());
        }

        // Add book management moved to "Manage Books" tab

        // Note: Create Account UI moved to the dedicated "User Management" tab.
        // Use the User Management tab button above to open account creation and management.
        ImGui::BeginChild("AdminBookGrid", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        LogExpanded("BEGINCHILD", "AdminBookGrid");

        // Admin: responsive grid for catalog
        float availAdmin = ImGui::GetContentRegionAvail().x;
        const float adminItemWidth = 160.0f;
        int adminCols = (std::max)(1, (int)(availAdmin / adminItemWidth));

        if (ImGui::BeginTable("admin_books_table", adminCols, ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {
            LogExpanded("BEGINTABLE", "admin_books_table");
            for (auto& book : books) {
                if (std::string(currentCategory) != "All" && book.category != currentCategory) continue;
                if (strlen(searchLibraryGlobal) > 0) {
                    std::string combined = book.title + " " + book.author + " " + book.category;
                    std::string combinedLow = combined;
                    std::string searchLow = searchLibraryGlobal;
                    std::transform(combinedLow.begin(), combinedLow.end(), combinedLow.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                    std::transform(searchLow.begin(), searchLow.end(), searchLow.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                    if (combinedLow.find(searchLow) == std::string::npos) continue;
                }

                ImGui::TableNextColumn();
                ImGui::PushID(book.title.c_str());
                LogExpanded("PUSHID", book.title.c_str());
                ImGui::BeginGroup();

                // Admin: improved card rendering (consistent with student view)
                static std::map<std::string, float> adminCoverScales;
                const float adminBaseW = 140.0f;
                const float adminMaxH = 200.0f;

                ID3D11ShaderResourceView* srv = nullptr;
                int texW = 0, texH = 0;
                if (!book.iconPath.empty()) {
                    if (g_bookTextures.count(book.iconPath)) {
                        srv = g_bookTextures[book.iconPath];
                        if (g_bookTextureSizes.count(book.iconPath)) { auto p = g_bookTextureSizes[book.iconPath]; texW = p.first; texH = p.second; }
                    } else {
                        if (LoadTextureFromFile(book.iconPath.c_str(), &srv, &texW, &texH)) {
                            g_bookTextures[book.iconPath] = srv;
                            g_bookTextureSizes[book.iconPath] = { texW, texH };
                        } else srv = nullptr;
                    }
                }

                float drawW = adminBaseW;
                float drawH = adminMaxH;
                if (srv && texW > 0 && texH > 0) {
                    float aspect = (float)texW / (float)texH;
                    drawW = adminBaseW;
                    drawH = drawW / aspect;
                    if (drawH > adminMaxH) { drawH = adminMaxH; drawW = drawH * aspect; }
                } else { drawW = adminBaseW; drawH = adminBaseW * 1.33f; if (drawH > adminMaxH) drawH = adminMaxH; }

                float& scale = adminCoverScales[book.title]; if (scale <= 0.001f) scale = 1.0f;
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 cursor = ImGui::GetCursorScreenPos();
                dl->AddRectFilled(ImVec2(cursor.x + 4, cursor.y + 6), ImVec2(cursor.x + drawW + 8, cursor.y + drawH + 10), IM_COL32(0,0,0,60), 6.0f);
                ImVec2 imgSize = ImVec2(drawW * scale, drawH * scale);
                if (srv) {
                    ImGui::Image((ImTextureID)srv, imgSize);
                    if (ImGui::IsItemClicked()) { selectedBookTitle = book.title; showBookDetailsPopup = true; }
                } else {
                    const char* ICON_BOOK = g_fontAwesomeLoaded ? u8"\uF02D" : "[BOOK]";
                    if (g_fontAwesomeLoaded && g_fontAwesome) { ImGui::PushFont(g_fontAwesome); if (ImGui::Button(ICON_BOOK, imgSize)) { selectedBookTitle = book.title; showBookDetailsPopup = true; } ImGui::PopFont(); }
                    else { if (ImGui::Button(ICON_BOOK, imgSize)) { selectedBookTitle = book.title; showBookDetailsPopup = true; } }
                }

                bool hovered = ImGui::IsItemHovered();
                float target = hovered ? 1.06f : 1.0f; float dt = ImGui::GetIO().DeltaTime; float speed = 10.0f; float alpha = ((dt * speed) < 1.0f ? (dt * speed) : 1.0f);
                scale = scale + (target - scale) * alpha;

                // Render status + metadata below the cover (mirrors student view styling)
                float overlayMaxW = imgSize.x;
                auto ellipsize = [&](const std::string& s, float maxW) {
                    if (ImGui::CalcTextSize(s.c_str()).x <= maxW) return s;
                    std::string t = s;
                    while (!t.empty() && ImGui::CalcTextSize((t + "...").c_str()).x > maxW) t.pop_back();
                    return t + "...";
                };
                std::string titleShort = ellipsize(book.title, overlayMaxW);
                std::string authorShort = ellipsize(book.author, overlayMaxW);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);

                bool isBorrowedAny = IsBookBorrowed(book.title);
                bool isBorrowedByAdmin = IsBookBorrowedBy(book.title, currentUsername);
                std::string statusText;
                if (isBorrowedByAdmin) statusText = "Borrowed";
                else if (isBorrowedAny) statusText = "Unavailable";
                else statusText = "Available";
                bool available = !isBorrowedAny;
                ImVec4 statusCol = available ? ImVec4(0.20f, 0.90f, 0.40f, 1.0f) : ImVec4(1.00f, 0.60f, 0.15f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, statusCol);
                ImGui::TextUnformatted(statusText.c_str());
                ImGui::PopStyleColor();

                bool adminFavorited = IsBookFavorited(adminUserStr, book.title);
                int adminWatchCount = GetFavoriteCountForBook(book.title);
                ImGui::Dummy(ImVec2(0, 2));
                if (adminUserStr.empty()) {
                    ImGui::BeginDisabled();
                    ImGui::SmallButton("Add Watch");
                    ImGui::EndDisabled();
                } else {
                    const char* watchLabel = adminFavorited ? "Remove Watch" : "Add Watch";
                    if (ImGui::SmallButton(watchLabel)) {
                        ToggleFavorite(adminUserStr, book.title);
                        adminFavorited = !adminFavorited;
                        adminWatchCount = GetFavoriteCountForBook(book.title);
                    }
                }
                if (adminFavorited) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.82f, 0.30f, 1.0f));
                    ImGui::TextUnformatted("On your watchlist");
                    ImGui::PopStyleColor();
                }
                if (adminWatchCount > 0) {
                    ImGui::TextDisabled("Watchers: %d", adminWatchCount);
                }

                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + overlayMaxW);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                ImGui::TextUnformatted(titleShort.c_str());
                ImGui::PopStyleColor();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.82f, 0.82f, 1.0f));
                ImGui::TextUnformatted(authorShort.c_str());
                ImGui::PopStyleColor();
                ImGui::PopTextWrapPos();

                ImGui::EndGroup();
                LogExpanded("BEFORE_POPID", book.title.c_str());
                ImGui::PopID();
                LogExpanded("AFTER_POPID", book.title.c_str());
            }
            LogExpanded("BEFORE_ENDTABLE", "admin_books_table");
            ImGui::EndTable();
            LogExpanded("AFTER_ENDTABLE", "admin_books_table");
        }
        LogExpanded("BEFORE_ENDCHILD", "AdminBookGrid");
        EndChildSafe();
        LogExpanded("AFTER_ENDCHILD", "AdminBookGrid");
    }
    else if (adminTab == 2) {
        // User Management tab (create accounts, view/edit users)
        ImGui::Separator();
        ImGui::Text("User Management");
        static char newUserName2[64] = "";
        static char newUserPass2[64] = "";
        static int newUserRole2 = 0; // 0 = student, 1 = admin
        static char newFullName2[128] = "";
        static char newSection2[64] = "";
        static char newGender2[32] = "";
        static int newAge2 = 0;

        int userMgmtTotalUsers = (int)users.size();
        int userMgmtAdminCount = 0;
        for (const auto& kv : users) {
            if (kv.second.role == "admin") {
                userMgmtAdminCount++;
            }
        }
        int userMgmtStudentCount = (std::max)(0, userMgmtTotalUsers - userMgmtAdminCount);
        std::string userMgmtAdminDetail = std::to_string(userMgmtAdminCount) + " admins";
        std::string userMgmtStudentDetail = std::to_string(userMgmtStudentCount) + " students";
        RenderStatCard("users_total", "Total Users", std::to_string(userMgmtTotalUsers), ImVec4(0.32f, 0.66f, 0.94f, 1.0f), 170.0f, userMgmtAdminDetail.c_str());
        ImGui::SameLine(0.0f, 10.0f);
        RenderStatCard("users_students", "Student Accounts", std::to_string(userMgmtStudentCount), ImVec4(0.31f, 0.82f, 0.55f, 1.0f), 170.0f, userMgmtStudentDetail.c_str());

        ImGui::Spacing();
        if (BeginSidebarCard("CreateUserCard", 0.0f, ImVec4(0.14f, 0.17f, 0.22f, 0.95f))) {
            ImGui::TextUnformatted("Create Account");
            ImGui::TextDisabled("Fill in the details below");
            ImGui::Separator();
            ImGui::InputText("Username", newUserName2, IM_ARRAYSIZE(newUserName2));
            ImGui::InputText("Password", newUserPass2, IM_ARRAYSIZE(newUserPass2), ImGuiInputTextFlags_Password);
            ImGui::Combo("Role", &newUserRole2, "student\0admin\0");
            ImGui::InputText("Full Name", newFullName2, IM_ARRAYSIZE(newFullName2));
            ImGui::InputText("Section", newSection2, IM_ARRAYSIZE(newSection2));
            ImGui::InputText("Gender", newGender2, IM_ARRAYSIZE(newGender2));
            ImGui::InputInt("Age", &newAge2);
            ImGui::Spacing();
            if (ImGui::Button("Create Account", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                std::string uname = trim(std::string(newUserName2));
                std::string pass = std::string(newUserPass2);
                std::string role = (newUserRole2 == 1) ? std::string("admin") : std::string("student");

                if (uname.empty() || pass.empty()) {
                    profileMessage = "ERROR: Username and password must not be empty.";
                }
                else if (users.count(uname)) {
                    profileMessage = "ERROR: Username already exists.";
                }
                else {
                    try {
                        std::ofstream ufile(usersCSV, std::ios::app);
                        if (ufile.is_open()) {
                            ufile << uname << "," << pass << "," << role << "\n";
                            ufile.close();
                        }

                        std::ofstream pfile(userProfileCSV, std::ios::app);
                        if (pfile.is_open()) {
                            pfile << uname << "," << std::string(newFullName2) << "," << std::string(newSection2) << "," << std::string(newGender2) << "," << newAge2 << "\n";
                            pfile.close();
                        }

                        LoadUsersFromCSV();

                        newUserName2[0] = '\0';
                        newUserPass2[0] = '\0';
                        newFullName2[0] = '\0';
                        newSection2[0] = '\0';
                        newGender2[0] = '\0';
                        newAge2 = 0;
                        newUserRole2 = 0;

                        profileMessage = std::string("Created user: ") + uname + " (" + role + ")";
                    }
                    catch (...) {
                        profileMessage = "ERROR: Failed to write user files.";
                    }
                }
            }
        }
        EndSidebarCard();

        ImGui::Spacing();
        // Existing users list + search/filter
        ImGui::Text("Existing Users");

        static char userSearch[128] = "";
        static int userFilterRole = 0; // 0=All,1=Student,2=Admin
        ImGui::InputTextWithHint("##userSearch", "Search users (username or full name)...", userSearch, IM_ARRAYSIZE(userSearch));
        ImGui::SameLine();
        ImGui::PushItemWidth(140);
        ImGui::Combo("##userRoleFilter", &userFilterRole, "All\0Student\0Admin\0");
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Reset Filters")) { userSearch[0] = '\0'; userFilterRole = 0; }

        bool userListVisible = ImGui::BeginChild("UserList", ImVec2(0, 260), true);
        if (userListVisible) {
            LogExpanded("BEGINCHILD", "UserList");
            std::vector<std::string> userKeys;
            userKeys.reserve(users.size());
            for (const auto &pair : users) userKeys.push_back(pair.first);
            std::sort(userKeys.begin(), userKeys.end(), [](const std::string &a, const std::string &b){
                std::string la = a; std::string lb = b;
                auto toLowerChar = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
                std::transform(la.begin(), la.end(), la.begin(), toLowerChar);
                std::transform(lb.begin(), lb.end(), lb.begin(), toLowerChar);
                return la < lb;
            });

            std::string searchLow = userSearch;
            std::transform(searchLow.begin(), searchLow.end(), searchLow.begin(), [](unsigned char c){ return (char)std::tolower(c); });
            int shown = 0;

            if (ImGui::BeginTable("users_admin_table", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableSetupColumn("Username", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Full Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Section", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableHeadersRow();

                for (const auto &uname : userKeys) {
                    const UserProfile &p = users.at(uname);
                    if (userFilterRole == 1 && p.role != "student") continue;
                    if (userFilterRole == 2 && p.role != "admin") continue;

                    if (!searchLow.empty()) {
                        std::string hay = uname + " " + p.name + " " + p.role + " " + p.section;
                        std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                        if (hay.find(searchLow) == std::string::npos) continue;
                    }

                    shown++;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(uname.c_str());

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextWrapped(p.name.empty() ? "N/A" : p.name.c_str());

                    ImGui::TableSetColumnIndex(2);
                    ImVec4 roleColor = (p.role == "admin") ? ImVec4(0.95f, 0.76f, 0.32f, 1.0f) : ImVec4(0.70f, 0.86f, 0.65f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, roleColor);
                    ImGui::TextUnformatted(p.role.c_str());
                    ImGui::PopStyleColor();

                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextDisabled("%s", p.section.empty() ? "N/A" : p.section.c_str());

                    ImGui::TableSetColumnIndex(4);
                    ImGui::PushID(uname.c_str());
                    
                    // Center the delete button in the column
                    float buttonWidth = ImGui::CalcTextSize("Delete").x + ImGui::GetStyle().FramePadding.x * 2.0f;
                    float availWidth = ImGui::GetContentRegionAvail().x;
                    if (buttonWidth < availWidth) {
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availWidth - buttonWidth) * 0.5f);
                    }
                    
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
                    
                    if (ImGui::Button((std::string("Delete##") + uname).c_str())) {
                        if (currentUsername && uname == currentUsername) {
                            profileMessage = "ERROR: You cannot delete the account you are currently using.";
                        } else {
                            if (RemoveUser(uname)) {
                                profileMessage = std::string("Deleted user: ") + uname;
                            } else {
                                profileMessage = std::string("ERROR: Failed to delete ") + uname;
                            }
                        }
                    }
                    
                    ImGui::PopStyleColor(3);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }

            ImGui::Text("Showing %d of %d users", shown, userMgmtTotalUsers);
        }
        LogExpanded("BEFORE_ENDCHILD", "UserList");
        EndChildSafe();
        LogExpanded("AFTER_ENDCHILD", "UserList");
        ImGui::Separator();
    }
    else if (adminTab == 1) { // Borrow History (admin)
        static char historySearch[128] = "";
        static int historyStatusFilter = 0; // 0=All,1=Borrowed,2=Returned
        static bool historySortNewest = true;
        static bool historyOnlyOverdue = false;
        const int overdueLimitDays = 14;

        int totalEntries = (int)borrowHistory.size();
        int activeBorrowed = 0;
        std::unordered_set<std::string> uniqueBorrowers;
        std::unordered_set<std::string> uniqueBooks;
        for (const auto& h : borrowHistory) {
            uniqueBorrowers.insert(h.borrowerName);
            uniqueBooks.insert(h.bookTitle);
            if (!h.isReturned) activeBorrowed++;
        }
        std::string totalDetail = std::to_string((int)uniqueBooks.size()) + " unique titles";
        std::string activeDetail = activeBorrowed > 0 ? "Pending pickups" : "All returned";
        std::string borrowerDetail = std::to_string((int)uniqueBorrowers.size()) + " borrowers";

        RenderStatCard("history_stat_total", "History Entries", std::to_string(totalEntries), ImVec4(0.28f, 0.62f, 0.98f, 1.0f), 180.0f, totalDetail.c_str());
        ImGui::SameLine(0.0f, 10.0f);
        RenderStatCard("history_stat_active", "Active Borrows", std::to_string(activeBorrowed), ImVec4(1.00f, 0.58f, 0.30f, 1.0f), 180.0f, activeDetail.c_str());
        ImGui::SameLine(0.0f, 10.0f);
        RenderStatCard("history_stat_users", "Borrowers", std::to_string((int)uniqueBorrowers.size()), ImVec4(0.52f, 0.78f, 0.46f, 1.0f), 180.0f, borrowerDetail.c_str());

        ImGui::Spacing();
        ImGui::Text("System Borrow History (%zu total entries)", borrowHistory.size());
        if (!profileMessage.empty()) {
            ImGui::TextWrapped("%s", profileMessage.c_str());
        }

        ImGui::InputTextWithHint("##historySearch", "Search book, borrower or code...", historySearch, IM_ARRAYSIZE(historySearch));
        ImGui::SameLine();
        ImGui::PushItemWidth(140);
        ImGui::Combo("##historyStatus", &historyStatusFilter, "All\0Borrowed\0Returned\0");
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::Checkbox("Newest first", &historySortNewest);
        ImGui::SameLine();
        ImGui::Checkbox("Only overdue", &historyOnlyOverdue);

        auto daysSince = [](const std::string& dateStr) -> int {
            std::tm tm = {};
            std::stringstream ss(dateStr);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            if (ss.fail()) return 0;
            std::time_t then = std::mktime(&tm);
            std::time_t now = std::time(nullptr);
            double diff = std::difftime(now, then);
            return (int)(diff / 86400.0);
        };

        std::string historySearchLow = historySearch;
        std::transform(historySearchLow.begin(), historySearchLow.end(), historySearchLow.begin(), [](unsigned char c){ return (char)std::tolower(c); });

        std::vector<int> historyRows;
        historyRows.reserve(borrowHistory.size());
        auto considerRow = [&](int idx) {
            const auto& h = borrowHistory[idx];
            bool matches = true;
            if (!historySearchLow.empty()) {
                std::string hay = h.bookTitle + " " + h.borrowerName + " " + h.returnCode;
                std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                if (hay.find(historySearchLow) == std::string::npos) matches = false;
            }
            if (historyStatusFilter == 1 && h.isReturned) matches = false;
            if (historyStatusFilter == 2 && !h.isReturned) matches = false;
            if (historyOnlyOverdue) {
                bool overdue = (!h.isReturned) && (daysSince(h.borrowDate) > overdueLimitDays);
                if (!overdue) matches = false;
            }
            if (matches) historyRows.push_back(idx);
        };

        if (historySortNewest) {
            for (int i = (int)borrowHistory.size() - 1; i >= 0; --i) considerRow(i);
        } else {
            for (int i = 0; i < (int)borrowHistory.size(); ++i) considerRow(i);
        }

        ImGui::Text("Showing %d entries", (int)historyRows.size());
        ImGui::Separator();

        // Printable return codes modal
        if (ImGui::Button("Printable Return Codes")) {
            ImGui::OpenPopup("Return Codes");
        }
        if (ImGui::BeginPopupModal("Return Codes", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            std::string out;
            out.reserve(borrowHistory.size() * 80);
            out += std::string("Return Codes Export\nGenerated: ") + __TIME__ + " " + __DATE__ + "\n\n";
            for (const auto& h : borrowHistory) {
                out += h.bookTitle + " , " + h.borrowerName + " , " + h.returnCode;
                out += h.isReturned ? " , Returned\n" : " , Borrowed\n";
            }
            ImGui::TextWrapped("Copy the list below and print using your preferred app.");
            ImGui::Separator();
            ImGui::BeginChild("print_codes_child", ImVec2(600, 300), true);
            ImGui::TextUnformatted(out.c_str());
            EndChildSafe();
            static bool pdfSaved = false;
            static std::string pdfSavedPath;
            if (ImGui::Button("Copy All")) {
                ImGui::SetClipboardText(out.c_str());
            }
            ImGui::SameLine();
            if (ImGui::Button("Save PDF")) {
                std::string fpath = basePath + "return_codes.pdf";
                pdfSaved = SaveReturnCodesPDF(fpath);
                if (pdfSaved) pdfSavedPath = fpath; else pdfSavedPath.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save to file")) {
                std::string fpath = basePath + "return_codes.txt";
                std::ofstream fout(fpath);
                if (fout.is_open()) { fout << out; fout.close(); }
            }
            ImGui::SameLine();
            if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
            if (!pdfSavedPath.empty()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Saved PDF: %s", pdfSavedPath.c_str());
            }
            ImGui::EndPopup();
            ImGui::Separator();
        }
        ImGui::Separator();
        ImGui::BeginChild("HistoryTable", ImVec2(0, 0), false);
        LogExpanded("BEGINCHILD", "HistoryTable");

        if (ImGui::BeginTable("history_table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            LogExpanded("BEGINTABLE", "history_table");
            ImGui::TableSetupColumn("Book Title", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Borrower", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Borrow Date", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Status / Return Date", ImGuiTableColumnFlags_WidthFixed, 200.0f);
            ImGui::TableSetupColumn("Return Code", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableHeadersRow();

            for (int idx : historyRows) {
                const auto& h = borrowHistory[idx];
                ImGui::TableNextRow();

                int daysElapsed = daysSince(h.borrowDate);
                bool overdue = (!h.isReturned) && (daysElapsed > overdueLimitDays);
                if (overdue) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(120, 40, 40, 80));
                }

                ImGui::TableSetColumnIndex(0); ImGui::Text("%s", h.bookTitle.c_str());

                //  Admin can click borrower name to view profile (read-only)
                ImGui::TableSetColumnIndex(1);
                ImGui::PushID(idx);
                LogExpanded("PUSHID", std::to_string(idx).c_str());
                if (ImGui::Button(h.borrowerName.c_str())) {
                    if (users.count(h.borrowerName)) {
                        profileViewUser = users[h.borrowerName];
                        forceProfileBufferReload = true;
                        showProfilePopup = true;
                    }
                    else {
                        profileMessage = "ERROR: User profile not found.";
                    }
                }
                LogExpanded("BEFORE_POPID", std::to_string(idx).c_str());
                ImGui::PopID();
                LogExpanded("AFTER_POPID", std::to_string(idx).c_str());

                ImGui::TableSetColumnIndex(2); ImGui::Text("%s", h.borrowDate.c_str());

                ImGui::TableSetColumnIndex(3);
                if (h.isReturned) {
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Returned: %s", h.returnDate.c_str());
                }
                else {
                    std::string timeElapsed = CalculateTimeElapsed(h.borrowDate);
                    ImVec4 statusCol = overdue ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : ImVec4(1.0f, 0.6f, 0.0f, 1.0f);
                    ImGui::TextColored(statusCol, overdue ? "Overdue" : "Still Borrowed");
                    ImGui::TextDisabled("(Elapsed: %s)", timeElapsed.c_str());
                    if (overdue) {
                        ImGui::TextDisabled("> %d days", daysElapsed);
                    }
                }

                ImGui::TableSetColumnIndex(4);
                ImGui::BeginGroup();
                ImGui::Text("%s", h.returnCode.c_str());
                ImGui::SameLine();
                // Per-entry printable PDF
                std::string btnLabel = std::string("Print Code##") + std::to_string(idx);
                if (ImGui::SmallButton(btnLabel.c_str())) {
                    // sanitize filename components
                    auto sanitize = [](const std::string& s) {
                        std::string o; for (char c : s) { if (std::isalnum((unsigned char)c) || c=='_' ) o += c; else if (std::isspace((unsigned char)c)) o += '_'; }
                        return o;
                    };
                    std::string bsan = sanitize(h.bookTitle);
                    std::string userSan = sanitize(h.borrowerName);
                    std::ostringstream defaultName;
                    defaultName << "return_code_" << userSan << "_" << bsan << "_" << idx << ".pdf";

                    std::string chosenPath;
                    if (!PickSaveFilePath(chosenPath, defaultName.str())) {
                        // user cancelled
                        profileMessage = "Save cancelled.";
                    }
                    else {
                        bool ok = SaveSingleReturnCodePDF(h, chosenPath);
                        if (ok) {
                            profileMessage = std::string("Saved return code PDF: ") + chosenPath;
                            // Open the PDF so admin can print it immediately
                            ShellExecuteA(NULL, "open", chosenPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        } else {
                            profileMessage = std::string("Failed to save PDF: ") + chosenPath;
                        }
                    }
                }
                // Place Mark Returned on its own line to ensure visibility
                if (!h.isReturned) {
                    std::string markLabel = std::string("Mark Returned##mr") + std::to_string(idx);
                    if (ImGui::SmallButton(markLabel.c_str())) {
                        AdminMarkEntryReturned(idx);
                    }
                }
                ImGui::EndGroup();
            }
            LogExpanded("BEFORE_ENDTABLE", "history_table");
            ImGui::EndTable();
            LogExpanded("AFTER_ENDTABLE", "history_table");
        }
        LogExpanded("BEFORE_ENDCHILD", "HistoryTable");
        EndChildSafe();
        LogExpanded("AFTER_ENDCHILD", "HistoryTable");
    }

    else if (adminTab == 3) {
        // Manage Books tab: Add new books and remove existing ones
        ImGui::Separator();
        ImGui::Text("Borrow Management");
        static char m_newTitle[256] = "";
        static char m_newAuthor[256] = "";
        static char m_newCategory[128] = "";
        static char m_newIcon[512] = "";
        static char m_libraryPath_local[512] = "";
        static char manageSearch[128] = "";
        static int manageStatusFilter = 0; // 0=All,1=Available,2=Borrowed

        std::string manageCatalogDetail = std::to_string(categoriesCount) + " categories";
        RenderStatCard("manage_books_catalog", "Catalog Size", std::to_string(totalBooks), ImVec4(0.31f, 0.66f, 0.92f, 1.0f), 180.0f, manageCatalogDetail.c_str());
        ImGui::SameLine(0.0f, 10.0f);
        RenderStatCard("manage_books_active", "Borrowed", std::to_string(borrowedNow), ImVec4(1.00f, 0.58f, 0.30f, 1.0f), 180.0f, "Currently out");
        ImGui::SameLine(0.0f, 10.0f);
        RenderStatCard("manage_books_available", "Available", std::to_string(availableNow), ImVec4(0.28f, 0.86f, 0.52f, 1.0f), 180.0f, "Ready to lend");

        ImGui::Spacing();
        float importBaseHeight = ImGui::GetFrameHeightWithSpacing() * 4.0f + ImGui::GetTextLineHeightWithSpacing() * 5.0f;
        if (!manageBooksStatusMessage.empty()) {
            ImVec2 textSize = ImGui::CalcTextSize(manageBooksStatusMessage.c_str(), nullptr, true);
            importBaseHeight += textSize.y + ImGui::GetStyle().ItemSpacing.y * 2.0f;
        }
        float importCardHeight = (std::max)(importBaseHeight, 210.0f);
        if (BeginSidebarCard("ImportLibraryCard", importCardHeight, ImVec4(0.14f, 0.18f, 0.23f, 0.95f))) {
            ImGui::TextUnformatted("Import / Sync");
            ImGui::TextDisabled("Pull in a catalog CSV to refresh the list");
            ImGui::Separator();
            ImGui::InputTextWithHint("##manage_libraryPath", "CSV path for import (optional)", m_libraryPath_local, IM_ARRAYSIZE(m_libraryPath_local));
            ImGui::Spacing();
            if (ImGui::Button("Import CSV", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                std::string pathToUse = m_libraryPath_local[0] ? std::string(m_libraryPath_local) : booksCSV;
                std::vector<Book> incoming;
                if (!LoadBooksFromCSVPath(pathToUse, incoming)) {
                    manageBooksStatusMessage = std::string("Import failed: unable to read ") + pathToUse;
                    showImportPreview = false;
                } else {
                    pendingDiffResult = ComputeBookDiff(books, incoming);
                    pendingImportPath = pathToUse;
                    pendingImportSource = ImportPreviewSource::Manage;
                    showImportPreview = true;
                    ImGui::OpenPopup("CSV Import Preview");
                    manageBooksStatusMessage = std::string("Previewing changes from: ") + pathToUse;
                }
            }
            if (!manageBooksStatusMessage.empty()) {
                ImGui::Spacing();
                ImGui::TextWrapped("%s", manageBooksStatusMessage.c_str());
            }
        }
        EndSidebarCard();

        ImGui::Spacing();
        if (BeginSidebarCard("AddBookCard", 0.0f, ImVec4(0.16f, 0.20f, 0.26f, 0.95f))) {
            ImGui::TextUnformatted("Add New Book");
            ImGui::TextDisabled("Provide metadata and optional cover path");
            ImGui::Separator();
            ImGui::InputText("Title##m", m_newTitle, IM_ARRAYSIZE(m_newTitle));
            ImGui::InputText("Author##m", m_newAuthor, IM_ARRAYSIZE(m_newAuthor));
            ImGui::InputText("Category##m", m_newCategory, IM_ARRAYSIZE(m_newCategory));
            ImGui::InputTextWithHint("Icon Path##m", "e.g. icons/book1.png or absolute path", m_newIcon, IM_ARRAYSIZE(m_newIcon));
            ImGui::SameLine();
            if (ImGui::Button("Browse...##m", ImVec2(90, 0))) {
                char fileBuf[MAX_PATH] = {0};
                OPENFILENAMEA ofn;
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = NULL;
                ofn.lpstrFile = fileBuf;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp\0All Files\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileNameA(&ofn)) {
                    strncpy_s(m_newIcon, sizeof(m_newIcon), fileBuf, _TRUNCATE);
                    m_newIcon[sizeof(m_newIcon)-1] = '\0';
                }
            }
            if (m_newIcon[0] != '\0') {
                ID3D11ShaderResourceView* previewSrv = nullptr;
                int pw = 0, ph = 0;
                std::string iconKey = trim(std::string(m_newIcon));
                if (g_bookTextures.count(iconKey)) previewSrv = g_bookTextures[iconKey];
                else {
                    if (LoadTextureFromFile(iconKey.c_str(), &previewSrv, &pw, &ph)) {
                        g_bookTextures[iconKey] = previewSrv;
                        g_bookTextureSizes[iconKey] = std::make_pair(pw, ph);
                    } else previewSrv = nullptr;
                }
                if (previewSrv) {
                    ImGui::Text("Preview:");
                    ImGui::Image((ImTextureID)previewSrv, ImVec2(64, 96));
                }
            }
            ImGui::Spacing();
            if (ImGui::Button("Add Book", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                std::string titleStr = trim(std::string(m_newTitle));
                if (titleStr.empty()) {
                    manageBooksStatusMessage = "ERROR: Title is required.";
                } else {
                    // Copy book cover to icons folder and get relative path
                    std::string iconPath = trim(std::string(m_newIcon));
                    if (!iconPath.empty()) {
                        iconPath = CopyBookCoverToIcons(iconPath);
                    }
                    
                    Book b; 
                    b.title = titleStr; 
                    b.author = trim(std::string(m_newAuthor)); 
                    b.category = trim(std::string(m_newCategory)); 
                    b.iconPath = iconPath;
                    
                    books.push_back(b);
                    SaveBooksToCSV();
                    
                    // Log the book addition activity
                    LogActivity("admin", "Add Book", "Added book: " + b.title + " by " + b.author);
                    
                    manageBooksStatusMessage = std::string("Added book: ") + b.title;
                    m_newTitle[0] = m_newAuthor[0] = m_newCategory[0] = m_newIcon[0] = '\0';
                    LoadBooksFromCSV();
                }
            }
        }
        EndSidebarCard();

        ImGui::Spacing();
        ImGui::Text("Existing Books (remove with caution)");
        ImGui::InputTextWithHint("##manage_search", "Search (title, author, category)...", manageSearch, IM_ARRAYSIZE(manageSearch));
        ImGui::SameLine();
        ImGui::PushItemWidth(150);
        ImGui::Combo("##manage_status_filter", &manageStatusFilter, "All\0Available\0Borrowed\0");
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Clear Filters")) { manageSearch[0] = '\0'; manageStatusFilter = 0; }

        ImGui::BeginChild("ManageBooksList", ImVec2(0, 260), true);
        static std::string pendingRemoveBook = "";
        static bool remove_modal_fallback = false;
        static int manage_selected_book = -1;
        if (ImGui::BeginTable("manage_books_table", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Author", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn("Watchlist", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableHeadersRow();
            for (int i = 0; i < (int)books.size(); ++i) {
                const Book &b = books[i];
                if (manageSearch[0] != '\0') {
                    std::string hay = b.title + " " + b.author + " " + b.category;
                    std::string searchLow = manageSearch;
                    std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                    std::transform(searchLow.begin(), searchLow.end(), searchLow.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                    if (hay.find(searchLow) == std::string::npos) continue;
                }
                bool isBorrowed = adminBorrowedTitles.count(b.title) > 0;
                if (manageStatusFilter == 1 && isBorrowed) continue;
                if (manageStatusFilter == 2 && !isBorrowed) continue;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool was_selected = (manage_selected_book == i);
                std::string titleLabel = b.title + std::string("##sel_") + std::to_string(i);
                if (ImGui::Selectable(titleLabel.c_str(), was_selected, 0, ImVec2(0,0))) {
                    manage_selected_book = i;
                }
                ImGui::TextDisabled("%s", b.category.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(b.author.c_str());
                ImGui::TableSetColumnIndex(2);
                ImVec4 statusCol = isBorrowed ? ImVec4(1.0f, 0.6f, 0.2f, 1.0f) : ImVec4(0.2f, 0.9f, 0.4f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, statusCol);
                ImGui::TextUnformatted(isBorrowed ? "Borrowed" : "Available");
                ImGui::PopStyleColor();
                ImGui::TableSetColumnIndex(3);
                int watchers = GetFavoriteCountForBook(b.title);
                if (watchers > 0) {
                    ImGui::Text("%d watching", watchers);
                } else {
                    ImGui::TextDisabled("No watchers");
                }
                ImGui::TableSetColumnIndex(4);
                ImGui::PushID(i);
                // Use a regular Button with a larger hit area and explicit ID
                if (ImGui::Button("Remove", ImVec2(100, 0))) {
                    pendingRemoveBook = b.title;
                    // Debug log click
                    if (g_enableDebugLogging) {
                        try {
                            std::string __log = GetExeDirGlobal() + "\\idstack_debug.log";
                            std::ofstream __o(__log, std::ios::app);
                            if (__o.is_open()) { __o << "MANAGE_REMOVE_CLICK title=" << b.title << " index=" << i << "\n"; __o.close(); }
                        } catch(...) {}
                    }
                    // ensure modal or fallback will open
                    ImGui::OpenPopup("ConfirmRemoveBook");
                    remove_modal_fallback = true;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        // Remove Selected control
        ImGui::Spacing();
        if (manage_selected_book >= 0 && manage_selected_book < (int)books.size()) {
            ImGui::Text("Selected: %s", books[manage_selected_book].title.c_str());
            ImGui::SameLine();
            if (ImGui::Button("Remove Selected", ImVec2(140,0))) {
                pendingRemoveBook = books[manage_selected_book].title;
                ImGui::OpenPopup("ConfirmRemoveBook");
                remove_modal_fallback = true;
            }
        }

        // Confirm removal modal
        // Log whether ImGui thinks the popup is open
        if (g_enableDebugLogging) {
            try {
                std::string __log = GetExeDirGlobal() + "\\idstack_debug.log";
                std::ofstream __o(__log, std::ios::app);
                if (__o.is_open()) { __o << "CONFIRM_REMOVE_POPUP_OPEN? " << (ImGui::IsPopupOpen("ConfirmRemoveBook")?"1":"0") << " pending=\"" << pendingRemoveBook << "\"\n"; __o.close(); }
            } catch(...) {}
        }

        if (ImGui::BeginPopupModal("ConfirmRemoveBook", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Remove book: %s\nThis action cannot be undone.", pendingRemoveBook.c_str());
            ImGui::Separator();
            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                if (!pendingRemoveBook.empty()) RemoveBook(pendingRemoveBook);
                pendingRemoveBook.clear();
                remove_modal_fallback = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                pendingRemoveBook.clear();
                remove_modal_fallback = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        else {
            // If popup failed to open (possibly due to another modal), show a fallback window
            if (remove_modal_fallback && !pendingRemoveBook.empty()) {
                ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);
                ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f - 210.0f, ImGui::GetIO().DisplaySize.y * 0.5f - 40.0f), ImGuiCond_Appearing);
                if (ImGui::Begin("Confirm Remove (Fallback)", &remove_modal_fallback, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::Text("Remove book: %s\nThis action cannot be undone.", pendingRemoveBook.c_str());
                    ImGui::Separator();
                    if (ImGui::Button("Delete", ImVec2(120, 0))) {
                        if (!pendingRemoveBook.empty()) RemoveBook(pendingRemoveBook);
                        pendingRemoveBook.clear();
                        remove_modal_fallback = false;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                        pendingRemoveBook.clear();
                        remove_modal_fallback = false;
                    }
                }
                ImGui::End();
            }
        }
        ImGui::EndChild();
    }
    else if (adminTab == 4) { // Activity Logs
        ImGui::Text(" Activity Logs - User Actions & System Events");
        ImGui::Separator();
        
        // Filters
        static char logSearchUser[128] = "";
        static char logSearchAction[128] = "";
        static int logTimeFilter = 0; // 0=All, 1=Today, 2=This Week, 3=This Month
        
        ImGui::InputTextWithHint("##logSearchUser", "Filter by username...", logSearchUser, IM_ARRAYSIZE(logSearchUser));
        ImGui::SameLine();
        ImGui::InputTextWithHint("##logSearchAction", "Filter by action...", logSearchAction, IM_ARRAYSIZE(logSearchAction));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::Combo("##logTimeFilter", &logTimeFilter, "All Time\0Today\0This Week\0This Month\0");
        
        ImGui::SameLine();
        if (ImGui::Button("Export to PDF", ImVec2(130, 0))) {
            // Filter logs based on current filters
            std::vector<ActivityLog> filteredLogs;
            std::string searchUser = trim(std::string(logSearchUser));
            std::string searchAction = trim(std::string(logSearchAction));
            std::transform(searchUser.begin(), searchUser.end(), searchUser.begin(), ::tolower);
            std::transform(searchAction.begin(), searchAction.end(), searchAction.begin(), ::tolower);
            
            for (const auto& log : activityLogs) {
                std::string userLower = log.username;
                std::string actionLower = log.action;
                std::transform(userLower.begin(), userLower.end(), userLower.begin(), ::tolower);
                std::transform(actionLower.begin(), actionLower.end(), actionLower.begin(), ::tolower);
                
                if (!searchUser.empty() && userLower.find(searchUser) == std::string::npos) continue;
                if (!searchAction.empty() && actionLower.find(searchAction) == std::string::npos) continue;
                
                filteredLogs.push_back(log);
            }
            
            std::string filename = "activity_log_report_" + GetCurrentDateTimeString();
            // Replace spaces and colons with underscores for valid filename
            for (char& c : filename) {
                if (c == ' ' || c == ':') c = '_';
            }
            ExportActivityLogsToPDF(filename, filteredLogs);
        }
        
        ImGui::Spacing();
        ImGui::Text("Total Activities: %d", (int)activityLogs.size());
        ImGui::Separator();
        
        // Display logs in a table
        ImGui::BeginChild("ActivityLogsTable", ImVec2(0, 0), true);
        if (ImGui::BeginTable("activity_logs_table", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthFixed, 160.0f);
                ImGui::TableSetupColumn("Username", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();
                
                std::string searchUser = trim(std::string(logSearchUser));
                std::string searchAction = trim(std::string(logSearchAction));
                std::transform(searchUser.begin(), searchUser.end(), searchUser.begin(), ::tolower);
                std::transform(searchAction.begin(), searchAction.end(), searchAction.begin(), ::tolower);
                
                // Show logs in reverse order (newest first)
                for (int i = (int)activityLogs.size() - 1; i >= 0; i--) {
                    const ActivityLog& log = activityLogs[i];
                    
                    // Apply filters
                    std::string userLower = log.username;
                    std::string actionLower = log.action;
                    std::transform(userLower.begin(), userLower.end(), userLower.begin(), ::tolower);
                    std::transform(actionLower.begin(), actionLower.end(), actionLower.begin(), ::tolower);
                    
                    if (!searchUser.empty() && userLower.find(searchUser) == std::string::npos) continue;
                    if (!searchAction.empty() && actionLower.find(searchAction) == std::string::npos) continue;
                    
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(log.timestamp.c_str());
                    
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(log.username.c_str());
                    
                    ImGui::TableSetColumnIndex(2);
                    // Color code actions
                    ImVec4 actionColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                    if (log.action == "Login") actionColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
                    else if (log.action == "Logout") actionColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                    else if (log.action == "Borrow") actionColor = ImVec4(0.3f, 0.6f, 1.0f, 1.0f);
                    else if (log.action == "Return") actionColor = ImVec4(0.2f, 0.8f, 0.5f, 1.0f);
                    else if (log.action == "Delete User" || log.action == "Remove Book") actionColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    else if (log.action == "Create User" || log.action == "Add Book") actionColor = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);
                    
                    ImGui::TextColored(actionColor, "%s", log.action.c_str());
                    
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextWrapped("%s", log.details.c_str());
                }
                
                ImGui::EndTable();
            }
            ImGui::EndChild();
    }

    if (showImportPreview && !ImGui::IsPopupOpen("CSV Import Preview")) {
        ImGui::OpenPopup("CSV Import Preview");
    }
    if (ImGui::BeginPopupModal("CSV Import Preview", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Import File: %s", pendingImportPath.empty() ? "(none)" : pendingImportPath.c_str());
        ImGui::Separator();
        ImGui::Text("Detected Changes: Added %d | Updated %d | Removed %d",
            pendingDiffResult.added,
            pendingDiffResult.updated,
            pendingDiffResult.removed);
        if (pendingDiffResult.entries.empty()) {
            ImGui::TextDisabled("No differences found. Importing will still replace the current catalog.");
        } else {
            ImVec2 tableSize = ImVec2(620, 260);
            if (ImGui::BeginTable("csv_diff_table", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchSame, tableSize)) {
                ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Author", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();
                for (size_t idx = 0; idx < pendingDiffResult.entries.size(); ++idx) {
                    const BookDiffEntry& entry = pendingDiffResult.entries[idx];
                    const Book& display = (entry.type == BookDiffType::Removed) ? entry.currentBook : entry.incomingBook;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImVec4 col = BookDiffTypeColor(entry.type);
                    ImGui::PushStyleColor(ImGuiCol_Text, col);
                    ImGui::TextUnformatted(BookDiffTypeLabel(entry.type));
                    ImGui::PopStyleColor();

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextWrapped("%s", display.title.c_str());

                    auto renderField = [&](const std::string& oldValue, const std::string& newValue, bool showArrow) {
                        if (entry.type == BookDiffType::Updated && showArrow && oldValue != newValue) {
                            ImGui::Text("%s -> %s", oldValue.c_str(), newValue.c_str());
                        } else {
                            const std::string& valueToUse = (entry.type == BookDiffType::Removed) ? oldValue : newValue;
                            ImGui::TextWrapped("%s", valueToUse.c_str());
                        }
                    };

                    ImGui::TableSetColumnIndex(2);
                    renderField(entry.currentBook.author, entry.incomingBook.author, true);

                    ImGui::TableSetColumnIndex(3);
                    renderField(entry.currentBook.category, entry.incomingBook.category, true);
                    if (entry.type == BookDiffType::Updated && entry.currentBook.iconPath != entry.incomingBook.iconPath) {
                        ImGui::TextDisabled("Icon: %s -> %s", entry.currentBook.iconPath.c_str(), entry.incomingBook.iconPath.c_str());
                    }
                }
                ImGui::EndTable();
            }
        }
        ImGui::Separator();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showImportPreview = false;
            pendingImportPath.clear();
            pendingDiffResult = BookDiffResult();
            pendingImportSource = ImportPreviewSource::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(pendingImportPath.empty());
        if (ImGui::Button("Apply Import", ImVec2(140, 0))) {
            bool ok = ImportBooksFromCSV(pendingImportPath);
            std::string resultMsg = ok ? std::string("Imported from: ") + pendingImportPath : std::string("Import failed");
            if (pendingImportSource == ImportPreviewSource::Manage) {
                manageBooksStatusMessage = resultMsg;
            } else if (pendingImportSource == ImportPreviewSource::Catalog) {
                catalogImportMessage = resultMsg;
            }
            if (ok) {
                LoadBooksFromCSV();
            }
            showImportPreview = false;
            pendingImportPath.clear();
            pendingDiffResult = BookDiffResult();
            pendingImportSource = ImportPreviewSource::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }

    LogExpanded("BEFORE_ENDCHILD", "AdminMainContent");
    EndChildSafe();
    LogExpanded("AFTER_ENDCHILD", "AdminMainContent");

    ImGui::End();

}


// ---------------- WinMain / ImGui ----------------
int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L,
        GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
        L"E-Library", NULL };
    ::RegisterClassExW(&wc);

    HWND hwnd = ::CreateWindowW(
        wc.lpszClassName, L"E-Library System",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME,
        100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Start from dark theme then tweak for a cleaner, friendlier look
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    // Softer rounded windows and controls
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    // More comfortable spacing
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.DisplaySafeAreaPadding = ImVec2(8, 8);

    // Gentle color adjustments to the dark theme for better contrast
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.12f, 0.15f, 0.95f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.11f, 0.13f, 0.9f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.28f, 0.32f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.34f, 0.38f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.09f, 0.60f, 0.80f, 0.90f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.10f, 0.69f, 0.90f, 0.95f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.08f, 0.54f, 0.72f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.12f, 0.48f, 0.70f, 0.90f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.15f, 0.60f, 0.85f, 0.95f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.10f, 0.40f, 0.60f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.08f, 0.50f, 0.72f, 0.84f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.12f, 0.60f, 0.84f, 0.95f);
    colors[ImGuiCol_TabActive] = ImVec4(0.07f, 0.42f, 0.60f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.06f, 0.08f, 0.9f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.10f, 0.14f, 1.0f);

    // Try to load a system UI font for better readability on Windows
    // Fallback to default font if the file is not present.
    ImGuiIO& ioLocal = ImGui::GetIO();
    const char* winFontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
    ImFont* loadedFont = nullptr;
    try {
        if (std::filesystem::exists(winFontPath)) {
            loadedFont = ioLocal.Fonts->AddFontFromFileTTF(winFontPath, 17.0f);
        }
    } catch(...) {
        loadedFont = nullptr;
    }
    if (loadedFont) ioLocal.FontDefault = loadedFont;
    // Attempt to load Font Awesome (merge into default font so icons are available inline)
    try {
        // Candidate paths to look for Font Awesome files
        const char* faCandidates[] = {
            "./fonts/fa-solid-900.ttf",
            "./fonts/FontAwesome.ttf",
            "C:\\Windows\\Fonts\\FontAwesome.otf",
            "C:\\Windows\\Fonts\\fa-solid-900.ttf",
            NULL
        };

        ImFontConfig fa_cfg;
        fa_cfg.MergeMode = true; // merge into previous font (Segoe or default)
        fa_cfg.PixelSnapH = true;
        // Private Use Area where FontAwesome places icons (range U+F000..U+F3FF)
        static const ImWchar icons_ranges[] = { 0xF000, 0xF3FF, 0 };

        for (int i = 0; faCandidates[i] != NULL; ++i) {
            try {
                if (std::filesystem::exists(faCandidates[i])) {
                    ImFont* f = ioLocal.Fonts->AddFontFromFileTTF(faCandidates[i], 14.0f, &fa_cfg, icons_ranges);
                    if (f) {
                        g_fontAwesomeLoaded = true;
                        g_fontAwesome = f;
                        break;
                    }
                }
            } catch(...) {}
        }
        // If merge didn't work, try loading a standalone Font Awesome font (not merged)
        if (!g_fontAwesomeLoaded) {
            for (int i = 0; faCandidates[i] != NULL; ++i) {
                try {
                    if (std::filesystem::exists(faCandidates[i])) {
                        ImFont* f2 = ioLocal.Fonts->AddFontFromFileTTF(faCandidates[i], 16.0f);
                        if (f2) {
                            g_fontAwesomeLoaded = true;
                            g_fontAwesome = f2;
                            break;
                        }
                    }
                } catch(...) {}
            }
        }
    } catch(...) {}
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Resolve CSV paths relative to the executable directory so the app doesn't
    // depend on the process current working directory. This ensures the EXE
    // always reads the db files next to the executable (Release/db/...)
    {
        char exeBuf[MAX_PATH] = {0};
        if (GetModuleFileNameA(NULL, exeBuf, MAX_PATH) != 0) {
            std::filesystem::path exeP(exeBuf);
            std::string exeDir = exeP.parent_path().string();
            basePath = exeDir + "\\db\\";
            usersCSV = basePath + "users.csv";
            userProfileCSV = basePath + "usersprofile.csv";
            booksCSV = basePath + "booklist.csv";
            historyCSV = basePath + "borrow_history.csv";
            favoritesCSV = basePath + "favorites.csv";
        }
        else {
            // fallback to relative path if resolution failed
            basePath = ".\\db\\";
            usersCSV = basePath + "users.csv";
            userProfileCSV = basePath + "usersprofile.csv";
            booksCSV = basePath + "booklist.csv";
            historyCSV = basePath + "borrow_history.csv";
            favoritesCSV = basePath + "favorites.csv";
        }
    }

    EnsureCSVExists();
    ConnectToDatabase();
    LoadFineSettings();  // Load fine system settings
    LoadUsersFromCSV();
    LoadBooksFromCSV();
    LoadBorrowHistory();
    LoadFavoritesFromCSV();
    LoadActivityLogsFromCSV(); // Load activity logs

    // Load background logo (preserve alpha) from db/bglogo/bglogo.png if present.
    // Try Release/db first, then the repository example db folder if present.
    {
        ID3D11ShaderResourceView* logoSrv = nullptr;
        int lw = 0, lh = 0;
        bool loaded = false;

        // Candidate 1: Release/db/bglogo/bglogo.png (basePath is exeDir + "\\db\\")
        std::string logoPath = basePath + "bglogo\\bglogo.png";
        if (LoadTextureFromFile(logoPath.c_str(), &logoSrv, &lw, &lh)) {
            g_bookTextures["bglogo"] = logoSrv;
            g_bookTextureSizes["bglogo"] = { lw, lh };
            loaded = true;
        }

        // Candidate 2: repository example dir ../db/bglogo/bglogo.png (use exe parent)
        if (!loaded) {
            char exeBufLocal2[MAX_PATH] = { 0 };
            if (GetModuleFileNameA(NULL, exeBufLocal2, MAX_PATH) != 0) {
                std::filesystem::path exeP2(exeBufLocal2);
                std::filesystem::path repoExampleDir = exeP2.parent_path(); // Release/ -> example dir is parent
                // If Release is child of example dir, parent_path() yields example dir
                // but in some setups we need parent_path().parent_path(); be conservative
                if (repoExampleDir.filename() == "Release") repoExampleDir = repoExampleDir.parent_path();
                std::string repoLogo = repoExampleDir.string() + "\\db\\bglogo\\bglogo.png";
                if (LoadTextureFromFile(repoLogo.c_str(), &logoSrv, &lw, &lh)) {
                    g_bookTextures["bglogo"] = logoSrv;
                    g_bookTextureSizes["bglogo"] = { lw, lh };
                    loaded = true;
                }
            }
        }
    }

    bool done = false;
    bool loggedIn = false;
    static char username[64] = "";
    static char password[64] = "";
    ImVec4 clear_color = ImVec4(0.2f, 0.25f, 0.3f, 1.0f);

    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        auto fullSize = io.DisplaySize;

        // ---------------- LOGIN ROLE SECURITY ----------------
        if (!loggedIn) {
            RenderLoginScreen(fullSize.x, fullSize.y, loggedIn, username, password);
        }
        else {
            if (currentUser.role == "admin") {
                RenderAdminDashboard(fullSize.x, fullSize.y, loggedIn, username);
            }
            else {
                RenderStudentDashboard(fullSize.x, fullSize.y, loggedIn, username);
            }
        }

        // Render popups (always on top)
        if (showProfilePopup) {
            RenderProfilePopup(fullSize.x, fullSize.y, profileViewUser.username.empty() ? currentUser : profileViewUser);
        }

        if (showSettingsPopup) {
            RenderSettingsPopup(fullSize.x, fullSize.y);
        }

        if (showBookDetailsPopup) {
            RenderBookDetailsPopup(fullSize.x, fullSize.y, username);
        }

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// ---------------- DirectX / Win32 Implementation ----------------
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;
    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pd3dDeviceContext) g_pd3dDeviceContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
    // release loaded book textures
    CleanupBookTextures();
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) { g_ResizeWidth = (UINT)LOWORD(lParam); g_ResizeHeight = (UINT)HIWORD(lParam); }
        return 0;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
