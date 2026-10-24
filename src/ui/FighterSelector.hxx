#pragma once
#include "../common/FighterCatalog.hxx"
#include "../common/InstalledContent.hxx"
#include "../common/StageCatalog.hxx"
#include "SelectionArt.hxx"
#include "GameMenu.hxx"
#include <functional>
#include <string>
#include <vector>

namespace sf4e { namespace ui {
bool DrawStageSelector(int& nativeId, SelectionArt* art);
// The names every screen uses for a costume ("Original", "Alternate 1 / pack",
// a custom costume's installed name or "Custom 12") and an Ultra, so a saved
// choice reads the same as the card that made it.
std::string CostumeLabel(const selection::Pick& pick, const selection::InstalledContent* installed = nullptr);
const char* UltraLabel(int ultra);
// Home's short forms of the same: "Alt 1, color 3" (a pack costume by its
// pack's name alone, one costume each) and "U1", "U2" or "Double".
std::string ShortAppearanceLabel(const selection::Pick& pick, const selection::InstalledContent* installed = nullptr);
const char* ShortUltraLabel(int ultra);
// A stage's name (a custom stage's installed name, or "Custom C12"), or Random.
std::string StageLabel(int stageId, const selection::InstalledContent* installed = nullptr);
// The pinned combinations, owned by the caller so they save with the
// preferences. remembered gives a fighter's last combination, for adding from a
// roster card that is not the live pick; remember is told an applied pick so
// the fighter's own memory follows it. applied and listChanged are outputs for
// the frame: a favourite was applied, or the list was added to or removed from.
struct FavouritesAccess {
    std::vector<selection::Pick>* picks = nullptr;
    std::function<selection::Pick(int)> remembered;
    std::function<void(const selection::Pick&)> remember;
    bool applied = false;
    bool listChanged = false;
};
class FighterSelector {
public:
    enum class Page { Fighter, Appearance, Ultra, Stage, Favourites };
    Page CurrentPage() const { return page_; }
    MenuNavigation& Navigation() { return menu_.navigation; }
    using AvailabilityReader = std::function<selection::Availability(int)>;
    // selectionError explains why the current choice is not usable. The room
    // screens tell the player to open Fighter Select, so Fighter Select has to
    // be able to say what is wrong once they arrive.
    // randomPool, given with stageId, adds the Random stage pool page. installed
    // adds the custom costumes, colors and stages installed on this PC.
    bool Draw(selection::Pick& pick, bool editionSelect, SelectionArt* art, const AvailabilityReader& readAvailability,
              int* stageId = nullptr, bool editable = true, const std::string& selectionError = {},
              selection::RandomPool* randomPool = nullptr, const selection::InstalledContent* installed = nullptr,
              FavouritesAccess* favourites = nullptr);
private:
    GameMenu menu_;
    Page page_ = Page::Fighter;
    // The page opened for one change (EmbeddedReturn::openOn), or empty.
    std::string openOn_;
    // The roster card the footer's add action refers to. Focus is read before
    // the menu draws, so a card focused now is remembered for the next frame.
    int addFighter_ = -1;
    // The card whose star was clicked this frame; its row action is dropped.
    std::string starId_;
    // The screen the notice belongs to, so it does not follow to another page.
    std::string noticeScreen_;
    // The Ultra step was just pushed: focus the Ultra the new fighter has,
    // which the caller restores after this frame.
    bool focusSavedUltra_ = false;
    // A fighter was just picked. Whether its Ultra step is needed is decided
    // on the next frame, from the pick as the caller restored it for that
    // fighter (its own saved edition), not from the previous fighter's.
    bool ultraStepPending_ = false;
    std::string notice_;
};
} }
