#include "FighterSelector.hxx"
#include "Theme.hxx"
#include "MenuRows.hxx"
#include "../common/Localization.hxx"
#include "../common/StageCatalog.hxx"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace sf4e { namespace ui {
namespace {
using palette::Ember;
using palette::Ivory;
using palette::Muted;

// Motion icons are vector paths, so they retain their shape at every DPI.
void CommandSymbols(const char* notation) {
    std::istringstream stream(notation);
    std::vector<std::string> tokens;
    for (std::string token; stream >> token;) tokens.push_back(token);
    const float scale = Scale(), unit = 30 * scale, gap = 3 * scale;
    const float available = ImGui::GetContentRegionAvail().x;
    float x = 0, y = 0;
    const auto origin = ImGui::GetCursorScreenPos();
    auto* draw = ImGui::GetWindowDrawList();
    const auto arrow = [&](ImVec2 tip, ImVec2 direction) {
        const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (length < .001f) return;
        direction.x /= length; direction.y /= length;
        draw->AddTriangleFilled(tip,
            ImVec2(tip.x - 7 * scale * direction.x + 4 * scale * direction.y, tip.y - 7 * scale * direction.y - 4 * scale * direction.x),
            ImVec2(tip.x - 7 * scale * direction.x - 4 * scale * direction.y, tip.y - 7 * scale * direction.y + 4 * scale * direction.x), Ivory);
    };
    for (auto token : tokens) {
        const bool charge = token[0] == '~';
        if (charge) token.erase(0, 1);
        const bool triple = token == "PPP" || token == "KKK";
        const bool button = token.find('P') != std::string::npos || token.find('K') != std::string::npos;
        const float width = triple ? 2.4f * unit : token == "+" ? .6f * unit : unit;
        if (x > 0 && x + width > available) { x = 0; y += unit + 13 * scale; }
        const ImVec2 p(origin.x + x, origin.y + y), center(p.x + unit / 2, p.y + unit / 2);
        if (button) {
            const int count = triple ? 3 : 1;
            const char* label = token.c_str();
            char single[] = {token[0], 0};
            if (triple) label = single;
            for (int i = 0; i < count; ++i) {
                const ImVec2 c(center.x + i * unit * .7f, center.y);
                draw->AddCircleFilled(c, unit * .44f, IM_COL32(48, 36, 25, 255));
                draw->AddCircle(c, unit * .44f, Ember, 24, 1.5f * scale);
                const auto size = ImGui::CalcTextSize(label);
                draw->AddText(ImVec2(c.x - size.x / 2, c.y - size.y / 2), Ivory, label);
            }
        } else if (token == "+") {
            const auto size = ImGui::CalcTextSize("+");
            draw->AddText(ImVec2(p.x + (width - size.x) / 2, center.y - size.y / 2), Muted, "+");
        } else if (token.size() == 1) {
            const int digit = token[0] - '0';
            const ImVec2 direction(static_cast<float>((digit - 1) % 3 - 1), static_cast<float>(1 - (digit - 1) / 3));
            const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
            const ImVec2 d(direction.x / length, direction.y / length);
            const ImVec2 tip(center.x + 11 * scale * d.x, center.y + 11 * scale * d.y);
            draw->AddLine(ImVec2(center.x - 10 * scale * d.x, center.y - 10 * scale * d.y), tip, Ivory, 3 * scale);
            arrow(tip, d);
        } else {
            const float pi = 3.14159265f;
            draw->AddCircleFilled(center, unit * .48f, IM_COL32(47, 58, 59, 255));
            const float begin = token == "236" ? pi : token == "214" || token == "63214" ? 0.f : pi / 2;
            const float end = token == "236" || token == "214" ? pi / 2 : token == "63214" ? pi : -1.3f * pi;
            ImVec2 points[25];
            for (int i = 0; i < 25; ++i) {
                const float angle = begin + (end - begin) * i / 24;
                points[i] = ImVec2(center.x + 11 * scale * std::cos(angle), center.y + 11 * scale * std::sin(angle));
            }
            draw->AddPolyline(points, 25, Ivory, 0, 3 * scale);
            arrow(points[24], ImVec2(points[24].x - points[23].x, points[24].y - points[23].y));
            draw->AddCircleFilled(points[0], 2 * scale, Ember);
        }
        if (charge) {
            // A charge mark over the glyph; a longer translation shrinks to fit it.
            const char* hold = loc::T("selection.hold");
            const float fontSize = (std::min)(9 * scale, 9 * scale * width / (std::max)(1.f, ImGui::GetFont()->CalcTextSizeA(9 * scale, FLT_MAX, 0, hold).x));
            draw->AddText(ImGui::GetFont(), fontSize, ImVec2(p.x, p.y + unit), Ember, hold);
        }
        x += width + gap;
    }
    ImGui::Dummy(ImVec2((std::min)(available, x), y + unit + 12 * scale));
}
void UltraMoveInput(int fighterId, int ultra, int edition) {
    const auto* fighter = selection::FindFighter(fighterId);
    ImGui::TextWrapped("%s", fighter->ultras[ultra]);
    for (const auto& command : selection::UltraCommands(fighterId, ultra, edition)) {
        if (*command.condition) ImGui::TextWrapped("%s", command.condition);
        CommandSymbols(command.symbols);
        // The same input in words, with no left or right in it, for a player facing the other way.
        ImGui::TextDisabled("%s", selection::UltraNotation(command.symbols).c_str());
    }
}
void ImageInRect(const SelectionImage& image, ImVec2 min, ImVec2 max) {
    if (!image.texture || !image.width || !image.height) {
        const char* label=loc::T(image.missing?"selection.preview_unavailable":"selection.preview_loading");
        const auto size=ImGui::CalcTextSize(label);
        const float factor=(std::max)(0.f,(std::min)(1.f,(std::min)((max.x-min.x-8*Scale())/(std::max)(1.f,size.x),
            (max.y-min.y-8*Scale())/(std::max)(1.f,size.y))));
        ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),ImGui::GetFontSize()*factor,
            ImVec2((min.x+max.x-size.x*factor)*.5f,(min.y+max.y-size.y*factor)*.5f),Muted,label);
        return;
    }
    const float factor = (std::min)((max.x - min.x) / image.width, (max.y - min.y) / image.height);
    const ImVec2 size(image.width * factor, image.height * factor);
    const ImVec2 start(min.x + (max.x - min.x - size.x) / 2, min.y + (max.y - min.y - size.y) / 2);
    ImGui::GetWindowDrawList()->AddImage(image.texture, start, ImVec2(start.x + size.x, start.y + size.y), image.uvMin, image.uvMax);
}
bool ImageCard(const char* id, const char* label, const SelectionImage& image, ImVec2 size, bool selected) {
    ImGui::PushID(id);
    if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.32f, .17f, .08f, 1));
    const bool clicked = ImGui::Button("##choice", size);
    if (selected) ImGui::PopStyleColor();
    const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    auto* draw = ImGui::GetWindowDrawList();
    const float labelHeight = ImGui::GetTextLineHeight() + 6 * Scale();
    ImageInRect(image, ImVec2(min.x + 2, min.y + 2), ImVec2(max.x - 2, max.y - labelHeight));
    if (!image.texture) {
        const char* message = image.missing ? loc::T("selection.preview_unavailable") : "...";
        const auto text = ImGui::CalcTextSize(message);
        const float messageScale = (std::max)(0.f, (std::min)(1.f, (std::min)(
            (size.x - 8 * Scale()) / (std::max)(1.f, text.x),
            (size.y - labelHeight - 8 * Scale()) / (std::max)(1.f, text.y))));
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * messageScale,
            ImVec2(min.x + (size.x - text.x * messageScale) / 2,
                   min.y + (size.y - labelHeight - text.y * messageScale) / 2), Muted, message);
    }
    draw->AddRectFilled(ImVec2(min.x + 1, max.y - labelHeight), ImVec2(max.x - 1, max.y - 1), IM_COL32(20, 19, 18, 235));
    const auto text = ImGui::CalcTextSize(label);
    const float labelScale = (std::min)(1.f, (size.x - 6 * Scale()) / (std::max)(1.f, text.x));
    draw->PushClipRect(ImVec2(min.x + 3, max.y - labelHeight), ImVec2(max.x - 3, max.y), true);
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * labelScale,
                  ImVec2(min.x + (std::max)(3.f, (size.x - text.x * labelScale) / 2), max.y - labelHeight + 2 * Scale()),
                  selected ? Ember : Ivory, label);
    draw->PopClipRect();
    if (selected) draw->AddRect(min, max, Ember, 2 * Scale(), 0, 2 * Scale());
    // The preview is drawn over the button, so redraw focus above the image.
    if (ImGui::IsItemFocused()) draw->AddRect(ImVec2(min.x + 3, min.y + 3), ImVec2(max.x - 3, max.y - 3), Ivory, 2 * Scale(), 0, 2 * Scale());
    ImGui::PopID();
    return clicked;
}
SelectionImage Missing() { SelectionImage image; image.missing = true; return image; }
// A five-point star, filled for a pinned combination.
void Star(ImVec2 center, float radius, ImU32 color, bool filled) {
    ImVec2 points[10];
    for (int i = 0; i < 10; ++i) {
        const float angle = -3.14159265f / 2 + i * 3.14159265f / 5;
        const float r = i % 2 ? radius * .45f : radius;
        points[i] = ImVec2(center.x + std::cos(angle) * r, center.y + std::sin(angle) * r);
    }
    auto* draw = ImGui::GetWindowDrawList();
    if (filled) draw->AddConcavePolyFilled(points, 10, color);
    else draw->AddPolyline(points, 10, color, ImDrawFlags_Closed, 1.5f * Scale());
}
// The star over a card's corner. It is hit-tested by hand, after the card's own
// button, so it owns the press; the caller drops the card's action for that
// frame. A press is settled on release, which is when the card's button fires.
bool StarButton(ImVec2 at, float size, bool filled) {
    const ImVec2 mouse = ImGui::GetMousePos();
    const bool hovered = ImGui::IsWindowHovered() && mouse.x >= at.x && mouse.x < at.x + size &&
        mouse.y >= at.y && mouse.y < at.y + size;
    Star(ImVec2(at.x + size / 2, at.y + size / 2), size * .42f, hovered || filled ? palette::Ember : palette::Ivory, filled);
    return hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
}
// "Ken, Alternate 1, color 3, Ultra II, personal action 4", the combination a
// card or a footer action is about.
std::string FavouriteText(const selection::Pick& pick, const selection::InstalledContent* installed) {
    return loc::Tf("selection.favourite_detail", selection::FindFighter(pick.fighter)->name,
        CostumeLabel(pick, installed), pick.color + 1, UltraLabel(pick.ultra),
        pick.personalAction == 255 ? std::string(loc::T("common.none")) : std::to_string(pick.personalAction + 1));
}

int PreviewColor(int fighter,int costume,const selection::Availability& availability,const selection::InstalledContent* installed) {
    const auto colors=selection::AllowedColors(fighter,costume,availability,installed);
    return colors.empty()?0:colors.front();
}
const char* HandicapLabel(int handicap) {
    static const char* const ids[]={"selection.handicap_normal","selection.handicap_one_hit","selection.handicap_25","selection.handicap_50","selection.handicap_75"};
    return loc::T(ids[(std::max)(0,(std::min)(4,handicap))]);
}
// Where Select saves the focused card, the legend says what it saves.
MenuEntry Saving(MenuEntry row,const char* hint,bool editable) {
    if(editable)row.hint=loc::T(hint);
    return row;
}
}
std::string StageLabel(int stageId, const selection::InstalledContent* installed) {
    if (selection::IsRandomStage(stageId)) return loc::T("selection.random_stage");
    const int id = selection::NormalizeStage(stageId);
    if (const auto* stage = selection::FindStage(id)) return stage->name;
    // A custom stage is named by the setup program, or by its code.
    const std::string name = installed ? installed->StageName(id) : std::string();
    return name.empty() ? loc::Tf("selection.custom", selection::StageCode(id)) : name;
}
std::string CostumeLabel(const selection::Pick& pick, const selection::InstalledContent* installed) {
    if (pick.costume == 0) return loc::T("selection.original");
    // A custom costume is named by the setup program, or by its slot as it numbers them (slot = index + 1).
    if (selection::IsCustomCostume(pick.costume)) {
        const std::string name = installed ? installed->Name(pick.fighter, pick.costume) : std::string();
        return name.empty() ? loc::Tf("selection.custom", pick.costume + 1) : name;
    }
    return loc::Tf("selection.alternate_pack", pick.costume, selection::CostumePack(pick.fighter, pick.costume));
}
const char* UltraLabel(int ultra) {
    return loc::T(ultra==2?"selection.ultra_double":ultra==1?"selection.ultra_two":"selection.ultra_one");
}
std::string ShortAppearanceLabel(const selection::Pick& pick, const selection::InstalledContent* installed) {
    const std::string pack = selection::CostumePack(pick.fighter, pick.costume);
    const std::string costume = pick.costume > 0 && pack == "Alternate" ? loc::Tf("home.selection_alternate", pick.costume) :
        pick.costume > 0 && !pack.empty() ? pack : CostumeLabel(pick, installed);
    return loc::Tf("home.selection_appearance", costume, pick.color + 1);
}
const char* ShortUltraLabel(int ultra) {
    return loc::T(ultra==2?"home.selection_ultra_double":ultra==1?"home.selection_ultra_two":"home.selection_ultra_one");
}
namespace {
// "Ultra I: Metsu Hadoken", or "Ultra Double".
std::string UltraName(const selection::Pick& pick) {
    if(pick.ultra==2) return loc::T("selection.ultra_double");
    return std::string(UltraLabel(pick.ultra))+": "+selection::FindFighter(pick.fighter)->ultras[pick.ultra];
}
}



bool DrawStageSelector(int& nativeId, SelectionArt* art) {
    const int previous = nativeId;
    nativeId = selection::NormalizeStage(nativeId);
    const std::string selected = StageLabel(nativeId);
    const float scale = Scale(), width = ImGui::GetContentRegionAvail().x;
    const bool wide = width >= 800 * scale && ImGui::BeginTable("Stage layout", 2, ImGuiTableFlags_SizingStretchProp);
    if (wide) {
        ImGui::TableSetupColumn("Selected stage", ImGuiTableColumnFlags_WidthFixed, 280 * scale);
        ImGui::TableSetupColumn("Stages"); ImGui::TableNextColumn();
    }
    const float previewWidth = (std::min)(ImGui::GetContentRegionAvail().x, 360 * scale);
    const auto start = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(previewWidth, previewWidth * 9 / 16));
    ImageInRect(art ? art->Stage(nativeId) : Missing(), start, ImVec2(start.x + previewWidth, start.y + previewWidth * 9 / 16));
    ImGui::PushFont(HeadingFont()); ImGui::TextWrapped("%s", selected.c_str()); ImGui::PopFont();
    ImGui::TextDisabled("%s", loc::Tf("selection.stage_count", selection::VersusStageCount).c_str());
    ImGui::TextWrapped("%s", loc::T("selection.choose_battlefield"));
    if (wide) ImGui::TableNextColumn();
    else ImGui::Spacing();
    const int columns = (std::max)(1, (std::min)(4, static_cast<int>(ImGui::GetContentRegionAvail().x / (160 * scale))));
    if (ImGui::BeginTable("Stage cards", columns, ImGuiTableFlags_SizingStretchSame)) {
        for (const auto& stage : selection::StageList()) {
            ImGui::TableNextColumn();
            const float cardWidth = ImGui::GetContentRegionAvail().x;
            if (ImageCard(stage.code, stage.name, art ? art->Stage(stage.id) : Missing(),
                ImVec2(cardWidth, cardWidth * 9 / 16 + ImGui::GetTextLineHeight() + 6 * scale), stage.id == nativeId))
                nativeId = stage.id;
        }
        ImGui::EndTable();
    }
    if (wide) ImGui::EndTable();
    return previous != nativeId;
}


bool FighterSelector::Draw(selection::Pick& pick,bool editionSelect,SelectionArt* art,const AvailabilityReader& readAvailability,int* stageId,bool editable,const std::string& selectionError,selection::RandomPool* randomPool,const selection::InstalledContent* installed,FavouritesAccess* favourites) {
 using namespace selection;
 bool changed=false;auto& nav=menu_.navigation;
 if(favourites){favourites->applied=false;favourites->listChanged=false;}
 const bool hasFavourites=favourites&&favourites->picks;
 // The combination a roster card stands for: the live pick for the live
 // fighter, else that fighter's own last combination, fitted like a favourite.
 const auto rosterCandidate=[&](int fighter){
  Pick candidate=fighter==pick.fighter?pick:(favourites->remembered?favourites->remembered(fighter):pick);
  candidate.fighter=fighter;
  const auto fighterAvailability=readAvailability?readAvailability(fighter):Availability{};
  return FavouriteCandidate(candidate,editionSelect,fighterAvailability,installed);
 };
 const auto isFavourite=[&](const Pick& candidate){
  return hasFavourites&&std::find(favourites->picks->begin(),favourites->picks->end(),candidate)!=favourites->picks->end();
 };
 // A selector the shell has just opened starts on its first page, not on the
 // sub-page an earlier visit left it on; one it reshows resumes.
 // The parent says where Back from here goes and which of its shortcuts
 // this screen hands back to it.
 const auto& embedded=EmbeddedReturnContext();
 if(TakeEmbeddedFresh()){
  nav.Home();openOn_=embedded.openOn;
  if(openOn_=="roster"){nav.Push("roster");nav.Prefer("fighter-"+std::to_string(pick.fighter));}
  else if(openOn_=="ultra"){nav.Push("ultra");nav.Prefer("ultra-"+std::to_string(pick.ultra));}
  else if(openOn_=="costumes"){nav.Push("costumes");nav.Prefer("costume-"+std::to_string(pick.costume));}
  else if(openOn_=="stage"&&stageId){nav.Push("stage");nav.Prefer("stage-"+std::to_string(*stageId));}
  else if(openOn_=="options")nav.Push("options");
  else if(openOn_=="favourites")nav.Push("favourites");
  else openOn_.clear();
 }
 if(ultraStepPending_&&nav.Screen()=="ultra"&&AllowedUltras(pick.fighter,pick.edition).size()<=1){
  // The new fighter has one Ultra in its own edition: nothing to pick.
  nav.Return();focusSavedUltra_=false;
  if(!openOn_.empty())ForwardMenuAction({MenuAction::Close});else nav.Home();
 }
 ultraStepPending_=false;
 if(focusSavedUltra_&&nav.Screen()=="ultra")nav.Prefer("ultra-"+std::to_string(pick.ultra));
 focusSavedUltra_=false;
 const auto screen=nav.Screen();
 if(screen!=noticeScreen_){notice_.clear();noticeScreen_=screen;}
 menu_.rootName=loc::T("screen.selection");menu_.exitName=embedded.exitName;menu_.shortcutHints=embedded.shortcutHints;
 const auto availability=readAvailability?readAvailability(pick.fighter):Availability{};
 if(editable)changed=Normalize(pick,editionSelect,&availability,installed);
 static const std::vector<int> noStages;
 const std::vector<int>& customStages=installed?installed->stages:noStages;
 const char* locked=loc::T(editable?"selection.select_saves":"selection.locked_detail");
 // The option rows step with Left and Right and save as they change; Select does nothing on them.
 const char* adjusts=loc::T(editable?"selection.adjust_saves":"selection.locked_detail");
 std::vector<MenuEntry> rows;
 std::string title=loc::T("selection.title");int columns=1;float cardHeight=100;menu_.wideListShare=.53f;
 if(screen=="home"){
  rows={Row("roster",loc::T("selection.fighter"),FindFighter(pick.fighter)->name),Row("appearance",loc::T("selection.appearance"),loc::Tf("selection.appearance_value",CostumeLabel(pick,installed),pick.color+1)),
   Value("ultra",loc::T("selection.ultra_combo"),UltraName(pick),loc::T(editable?"selection.ultra_row.detail":"selection.locked_detail")),
   Row("stage",loc::T("selection.stage"),stageId?StageLabel(*stageId,installed):loc::T("selection.p1_stage"),stageId!=nullptr),
   Row("options",loc::T("selection.additional_options"),loc::T("selection.additional_options.detail"))};
  // Left and Right change the Ultra here; Select shows them with their inputs.
  rows[2].opens=true;rows[2].adjustable=editable&&AllowedUltras(pick.fighter,pick.edition).size()>1;
 }else if(screen=="roster"){
  page_=Page::Fighter;title=loc::T("selection.choose_fighter");
  // The pinned combinations lead the roster, as a card so the grid keeps its
  // first row whole.
  if(hasFavourites)rows.push_back(Row("favourites",loc::Tf("selection.favourites",favourites->picks->size()),
   loc::T("selection.favourites.detail")));
  // USFIV's own select order, 15 across when the list has the room for it.
  // A fighter the player's Steam licences do not cover keeps its place in the
  // grid, so the layout matches the game's, but cannot be saved.
  for(const int id:RosterDisplayOrder){
   const bool owned=!readAvailability||!FighterLocked(id,readAvailability(id));
   rows.push_back(Saving(Row("fighter-"+std::to_string(id),FindFighter(id)->name,owned?locked:loc::T("selection.not_owned"),editable&&owned),"menu.hint.save_fighter",editable&&owned));
  }
  // The footer saves the focused card's combination, not the live pick, so it
  // names it. It is not offered over the favourites card.
  if(hasFavourites){
   const std::string focus=nav.Focus();
   if(focus.compare(0,8,"fighter-")==0)addFighter_=std::stoi(focus.substr(8));
   if(focus.compare(0,8,"fighter-")==0||focus=="favourite-add"){
    const int target=addFighter_>=0?addFighter_:pick.fighter;
    const auto targetAvailability=readAvailability?readAvailability(target):Availability{};
    const Pick candidate=rosterCandidate(target);
    const bool owned=!readAvailability||!FighterLocked(target,targetAvailability);
    const bool available=owned&&Available(candidate,editionSelect,targetAvailability,installed);
    const bool already=isFavourite(candidate);
    const bool full=favourites->picks->size()>=static_cast<std::size_t>(FavouriteLimit);
    auto add=Row("favourite-add",loc::T("selection.favourite_add"),FavouriteText(candidate,installed),
     editable&&available&&!already&&!full);
    add.wide=true;
    if(already)add.detail+=std::string("\n")+loc::T("selection.favourite_duplicate");
    else if(full)add.detail+=std::string("\n")+loc::Tf("selection.favourite_full",FavouriteLimit);
    else if(!available)add.detail+=std::string("\n")+loc::T("runtime.selection_combination_unavailable");
    else add.hint=loc::T("menu.hint.add_favourite");
    rows.push_back(std::move(add));
   }
  }
  const auto grid=LayOutRosterGrid(ImGui::GetContentRegionAvail().x);
  columns=grid.columns;cardHeight=grid.cardHeight;menu_.wideListShare=grid.listShare;
 }else if(screen=="favourites"){
  page_=Page::Favourites;title=loc::T("selection.favourites_title");
  if(!hasFavourites||favourites->picks->empty()){
   rows.push_back(InfoRow("favourites-empty",loc::T("selection.favourites_empty"),std::string(),loc::T("selection.favourites_empty.detail")));
   rows.push_back(Row("roster",loc::T("selection.choose_fighter"),loc::T("selection.favourites_empty.roster")));
  }else{
   for(std::size_t i=0;i<favourites->picks->size();++i){
    const auto& f=(*favourites->picks)[i];
    const auto* who=FindFighter(f.fighter);
    const auto favAvailability=readAvailability?readAvailability(f.fighter):Availability{};
    const bool usable=Available(FavouriteCandidate(f,editionSelect,favAvailability,installed),editionSelect,favAvailability,installed);
    auto row=Row("favourite-"+std::to_string(i),who?who->name:loc::T("card.choose_fighter"),
     FavouriteText(f,installed),editable&&usable);
    if(editable&&usable)row.hint=loc::T("menu.hint.apply_favourite");
    else if(!usable)row.detail+=std::string("\n")+loc::T("runtime.selection_combination_unavailable");
    rows.push_back(std::move(row));
   }
   columns=(std::max)(2,(std::min)(5,static_cast<int>(ImGui::GetContentRegionAvail().x/(240*Scale()))));
   cardHeight=140;
  }
 }else if(screen=="appearance"){
  page_=Page::Appearance;title=loc::T("selection.appearance_title");
  rows={Row("costumes",loc::T("selection.costume_gallery"),CostumeLabel(pick,installed)),
   Row("colors",loc::T("selection.color_gallery"),loc::Tf("selection.color_preview",pick.color+1))};
 }else if(screen=="costumes"||screen=="colors"){
  page_=Page::Appearance;title=loc::T(screen=="costumes"?"selection.costume_gallery_title":"selection.color_gallery_title");
  if(!availability.ready)rows.push_back(Row("waiting",loc::T("selection.loading_choices"),loc::T("selection.loading_choices.detail"),false));
  else if(screen=="costumes")for(int costume:AllowedCostumes(pick.fighter,availability,installed)){
   auto option=pick;option.costume=costume;
   const bool usable=!AllowedColors(pick.fighter,costume,availability,installed).empty();
   // The legend says Select saves, so an editable card's detail is just its
   // pack, which fits the compact gallery's two lines with the saved choice.
   rows.push_back(Saving(Row("costume-"+std::to_string(costume),costume==0?loc::T("selection.original"):selection::IsCustomCostume(costume)?CostumeLabel(option,installed):loc::Tf("selection.alternate",costume),
    !usable?std::string(loc::T("selection.no_colors")):editable?CostumeLabel(option,installed):CostumeLabel(option,installed)+"\n"+locked,editable&&usable),"menu.hint.save_costume",editable));
  }else for(int color:AllowedColors(pick.fighter,pick.costume,availability,installed)){
   // A custom color is named by the setup program, else numbered like the rest.
   const std::string name=installed&&selection::IsCustomColor(color)?installed->Name(pick.fighter,pick.costume,color):std::string();
   rows.push_back(Saving(Row("color-"+std::to_string(color),name.empty()?loc::Tf("selection.color",color+1):name,editable?"":locked,editable),"menu.hint.save_color",editable));
  }
  columns=(std::max)(2,(std::min)(4,static_cast<int>(ImGui::GetContentRegionAvail().x/(300*Scale()))));
 }else if(screen=="ultra"){
  page_=Page::Ultra;title=loc::T("selection.ultra_combo_title");
  columns=(std::max)(1,(std::min)(3,static_cast<int>(ImGui::GetContentRegionAvail().x/(280*Scale()))));
  for(int ultra:AllowedUltras(pick.fighter,pick.edition)){
    auto row=Saving(Row("ultra-"+std::to_string(ultra),UltraLabel(ultra),
    ultra==2?loc::T("selection.ultra_double.detail"):FindFighter(pick.fighter)->ultras[ultra],editable),"menu.hint.save_ultra",editable);
   if(ultra==pick.ultra)row.value=loc::T("selection.saved");
   rows.push_back(std::move(row));
  }
 }else if(screen=="stage"){
  page_=Page::Stage;title=loc::T("selection.stage_title");
  rows.push_back(Saving(Row("stage-"+std::to_string(RandomStageId),loc::T("selection.random_stage"),stageId?loc::T("selection.random_stage.detail"):loc::T("selection.only_p1_stage"),editable&&stageId),"menu.hint.save_stage",editable&&stageId));
  for(const auto& stage:StageList())rows.push_back(Saving(Row("stage-"+std::to_string(stage.id),stage.name,stageId?locked:loc::T("selection.only_p1_stage"),editable&&stageId),"menu.hint.save_stage",editable&&stageId));
  for(int id:customStages)rows.push_back(Saving(Row("stage-"+std::to_string(id),StageLabel(id,installed),stageId?locked:loc::T("selection.only_p1_stage"),editable&&stageId),"menu.hint.save_stage",editable&&stageId));
  if(stageId&&randomPool){
   rows.push_back(Row("random-pool",loc::T("selection.random_pool"),loc::Tf("selection.random_pool.detail",randomPool->Count(customStages),VersusStageCount+static_cast<int>(customStages.size()))));
   rows.back().wide=true;
  }
  // Derive columns like the roster and the galleries do. A fixed three columns
  // left 16:9 stage cards far below the width the sibling grids guarantee.
  columns=(std::max)(2,(std::min)(4,static_cast<int>(ImGui::GetContentRegionAvail().x/(220*Scale()))));
 }else if(screen=="random-pool"){
  // Select takes a stage out of Random or puts it back; the last one stays.
  page_=Page::Stage;title=loc::T("selection.random_pool_title");
  // The game's stages, then the installed custom stages; the pool keeps the last game stage in.
  const RandomPool everything;
  const RandomPool& current=randomPool?*randomPool:everything;
  std::vector<int> ids;
  for(const auto& stage:StageList())ids.push_back(stage.id);
  ids.insert(ids.end(),customStages.begin(),customStages.end());
  for(int id:ids){
   const bool in=current.Contains(id),last=in&&!current.CanToggle(id);
   auto row=Row("pool-"+std::to_string(id),StageLabel(id,installed),loc::T(last?"selection.random_pool_last":in?"selection.random_pool_in":"selection.random_pool_out"),
    editable&&randomPool&&!last);
   if(row.enabled)row.hint=loc::T(in?"menu.hint.skip_stage":"menu.hint.include_stage");
   rows.push_back(std::move(row));
  }
  columns=(std::max)(2,(std::min)(4,static_cast<int>(ImGui::GetContentRegionAvail().x/(220*Scale()))));
 }else{
  title=loc::T("selection.options_title");
  rows={Value("edition",loc::T("selection.edition"),FindEdition(pick.edition)->name,editionSelect?adjusts:loc::T("selection.usfiv_rules"),editable&&editionSelect),
   Value("action",loc::T("selection.personal_action"),pick.personalAction==255?loc::T("common.none"):std::to_string(pick.personalAction+1),adjusts,editable&&availability.ready),
   Value("quote",loc::T("selection.win_quote"),pick.winQuote==255?loc::T("selection.random"):std::to_string(pick.winQuote+1),adjusts,editable),
   Value("handicap",loc::T("selection.handicap"),HandicapLabel(pick.handicap),adjusts,editable)};
 }
 const bool compactAppearance=(screen=="costumes"||screen=="colors")&&ImGui::GetContentRegionAvail().x<820*Scale();
 // What the footer's add action would pin, for its preview.
 const Pick addCandidate=hasFavourites&&screen=="roster"?rosterCandidate(addFighter_>=0?addFighter_:pick.fighter):Pick{};
 const auto preview=[&](const std::string& id){
  if(screen=="ultra"){
   ImGui::TextWrapped("%s",loc::Tf("selection.saved_value",UltraLabel(pick.ultra)).c_str());
   if(id.compare(0,6,"ultra-")==0){
    const int ultra=std::stoi(id.substr(6));
    ImGui::TextWrapped("%s",loc::T(ultra==pick.ultra?"selection.ultra_selected":editable?"selection.preview_save":"selection.preview_locked"));
    if(ultra==2){UltraMoveInput(pick.fighter,0,pick.edition);UltraMoveInput(pick.fighter,1,pick.edition);}
    else UltraMoveInput(pick.fighter,ultra,pick.edition);
   }
   return;
  }
  if(compactAppearance){
   ImGui::TextWrapped("%s",loc::Tf("selection.saved_appearance",CostumeLabel(pick,installed),pick.color+1).c_str());return;
  }
  // A favourite, or the combination the footer would pin: its whole pick, the
  // appearance included, not the live one.
  const Pick* favourite=nullptr;
  if(screen=="favourites"&&id.compare(0,10,"favourite-")==0)favourite=&(*favourites->picks)[std::stoi(id.substr(10))];
  else if(screen=="roster"&&id=="favourite-add")favourite=&addCandidate;
  if(favourite){
   ImGui::TextWrapped("%s",FavouriteText(*favourite,installed).c_str());
   // The edition is named even when the room's rule is off, where it is marked
   // as inactive because applying the favourite drops it.
   ImGui::TextDisabled("%s",editionSelect?FindEdition(favourite->edition)->name:
    (std::string(FindEdition(favourite->edition)->name)+" · "+loc::T("selection.favourite_rule_off")).c_str());
   const float width=(std::min)(ImGui::GetContentRegionAvail().x,300*Scale());
   const float height=(std::min)(ImGui::GetContentRegionAvail().y-10*Scale(),250*Scale());
   if(height>35*Scale()){
    const auto p=ImGui::GetCursorScreenPos();ImGui::Dummy(ImVec2(width,height));
    ImageInRect(art?art->Appearance(favourite->fighter,favourite->costume,favourite->color):Missing(),p,ImVec2(p.x+width,p.y+height));
   }
   return;
  }
  int focusFighter=pick.fighter,focusStage=stageId?*stageId:0,focusCostume=pick.costume,focusColor=pick.color;
  if(id.compare(0,8,"fighter-")==0)focusFighter=std::stoi(id.substr(8));
  if(id.compare(0,6,"stage-")==0)focusStage=std::stoi(id.substr(6));
  if(id.compare(0,5,"pool-")==0)focusStage=std::stoi(id.substr(5));
  if(id.compare(0,8,"costume-")==0){focusCostume=std::stoi(id.substr(8));focusColor=PreviewColor(pick.fighter,focusCostume,availability,installed);}
  if(id.compare(0,6,"color-")==0)focusColor=std::stoi(id.substr(6));
  ImGui::TextWrapped("%s",loc::Tf("selection.saved_full",FindFighter(pick.fighter)->name,CostumeLabel(pick,installed),pick.color+1).c_str());
  if(focusFighter!=pick.fighter||focusCostume!=pick.costume||focusColor!=pick.color)ImGui::TextWrapped("%s",loc::T("selection.preview_save_short"));
  const float width=(std::min)(ImGui::GetContentRegionAvail().x,300*Scale());
  const float height=(std::min)(ImGui::GetContentRegionAvail().y-10*Scale(),250*Scale());
  if(height>35*Scale()){
   const auto p=ImGui::GetCursorScreenPos();ImGui::Dummy(ImVec2(width,height));
   const auto img=!art?Missing():screen=="stage"||screen=="random-pool"?art->Stage(focusStage):(screen=="appearance"||screen=="costumes"||screen=="colors")?art->Appearance(pick.fighter,focusCostume,focusColor):art->Portrait(focusFighter,true);
   ImageInRect(img,p,ImVec2(p.x+width,p.y+height));
  }
 };
 GameMenu::Card card;
 const bool pool=screen=="random-pool";
 // An Ultra's card is its photo; Ultra Double is one large W on a dark panel.
 // A badge in the top-right corner names the Ultra (I, II or W); SAVED stays top-left.
 if(screen=="ultra")card=[&](const MenuEntry& e,ImVec2 min,ImVec2 max){
  const int ultra=std::stoi(e.id.substr(6));
  const float labelHeight=ImGui::GetTextLineHeight()+4*Scale();
  const ImVec2 top(min.x+3,min.y+3),bottom(max.x-3,max.y-labelHeight);
  if(ultra==2){
   auto* panel=ImGui::GetWindowDrawList();
   panel->AddRectFilled(top,bottom,IM_COL32(26,22,19,255));
   const float font=(std::max)(1.f,(std::min)((bottom.y-top.y)*.8f,(bottom.x-top.x)*.7f));
   const auto glyph=ImGui::GetFont()->CalcTextSizeA(font,FLT_MAX,0,"W");
   panel->AddText(ImGui::GetFont(),font,ImVec2((top.x+bottom.x-glyph.x)*.5f,(top.y+bottom.y-glyph.y)*.5f),palette::Ember,"W");
  }else ImageInRect(art?art->Ultra(pick.fighter,ultra):Missing(),top,bottom);
  DrawCardBadge(ImVec2(max.x-3,min.y+2),max.x-min.x-6,ultra==2?"W":ultra==1?"II":"I","ultra-badge",true);
  const bool saved=ultra==pick.ultra;
  const std::string label=ultra==2?e.label:e.label+": "+FindFighter(pick.fighter)->ultras[ultra];
  auto* d=ImGui::GetWindowDrawList();
  d->AddRectFilled(ImVec2(min.x,max.y-labelHeight),max,IM_COL32(16,15,14,230));
  const float font=(std::min)(ImGui::GetFontSize(),(max.x-min.x-6)*ImGui::GetFontSize()/(std::max)(1.f,ImGui::CalcTextSize(label.c_str()).x));
  d->AddText(ImGui::GetFont(),font,ImVec2(min.x+3,max.y-labelHeight),saved?palette::Ember:palette::Ivory,label.c_str());
  if(saved)DrawCardBadge(ImVec2(min.x+3,min.y+2),max.x-min.x-6,loc::T("selection.saved"),"saved-badge");
  return true;
 };
 if(screen=="roster"||screen=="stage"||pool||screen=="costumes"||screen=="colors")card=[&](const MenuEntry& e,ImVec2 min,ImVec2 max){
  if(e.id=="waiting"||e.wide)return false;
  if(e.id=="favourites"){
   // The pinned combinations' door: a big star and the count, no portrait.
   auto* d=ImGui::GetWindowDrawList();
   const float labelHeight=ImGui::GetTextLineHeight()+4*Scale();
   d->AddRectFilled(ImVec2(min.x+3,min.y+3),ImVec2(max.x-3,max.y-3),IM_COL32(26,22,19,255));
   const ImVec2 center((min.x+max.x)*.5f,(min.y+max.y-labelHeight)*.5f);
   const float radius=(std::min)(max.x-min.x,max.y-min.y-labelHeight)*.22f;
   const bool any=!favourites->picks->empty();
   Star(center,radius,any?palette::Ember:palette::Muted,any);
   const std::string count=loc::Tf("selection.favourites_count",static_cast<int>(favourites->picks->size()),FavouriteLimit);
   const auto size=ImGui::CalcTextSize(count.c_str());
   d->AddText(ImVec2(center.x-size.x*.5f,center.y+radius+2*Scale()),palette::Ivory,count.c_str());
   d->AddRectFilled(ImVec2(min.x,max.y-labelHeight),max,IM_COL32(16,15,14,230));
   const float font=(std::min)(ImGui::GetFontSize(),(max.x-min.x-6)*ImGui::GetFontSize()/(std::max)(1.f,ImGui::CalcTextSize(e.label.c_str()).x));
   d->AddText(ImGui::GetFont(),font,ImVec2(min.x+3,max.y-labelHeight),palette::Ivory,e.label.c_str());
   return true;
  }
  const int id=std::stoi(e.id.substr(screen=="roster"||screen=="costumes"?8:pool?5:6));
  const float labelHeight=ImGui::GetTextLineHeight()+4*Scale();
  const auto image=!art?Missing():screen=="roster"?art->Portrait(id):screen=="stage"||pool?art->Stage(id):
   art->Appearance(pick.fighter,screen=="costumes"?id:pick.costume,screen=="costumes"?PreviewColor(pick.fighter,id,availability,installed):id);
  ImageInRect(image,ImVec2(min.x+3,min.y+3),ImVec2(max.x-3,max.y-labelHeight));
  const bool saved=!pool&&(screen=="roster"?id==pick.fighter:screen=="costumes"?id==pick.costume:screen=="colors"?id==pick.color:stageId&&id==*stageId);
  ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(min.x,max.y-labelHeight),max,IM_COL32(16,15,14,230));
  const float font=(std::min)(ImGui::GetFontSize(),(max.x-min.x-6)*ImGui::GetFontSize()/(std::max)(1.f,ImGui::CalcTextSize(e.label.c_str()).x));
  ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),font,ImVec2(min.x+3,max.y-labelHeight),saved?palette::Ember:palette::Ivory,e.label.c_str());
  if(saved)DrawCardBadge(ImVec2(min.x+3,min.y+2),max.x-min.x-6,loc::T("selection.saved"),"saved-badge");
  if(screen=="roster"&&!saved&&readAvailability&&FighterLocked(id,readAvailability(id))){
   ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(min.x+3,min.y+3),ImVec2(max.x-3,max.y-labelHeight),IM_COL32(16,15,14,170));
   DrawCardBadge(ImVec2(min.x+3,min.y+2),max.x-min.x-6,loc::T("selection.not_owned_badge"),"not-owned-badge");
  }
  if(pool&&randomPool&&!randomPool->Contains(id)){
   // A skipped stage reads as switched off at a glance: dimmed, and labelled.
   ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(min.x+3,min.y+3),ImVec2(max.x-3,max.y-labelHeight),IM_COL32(16,15,14,170));
   DrawCardBadge(ImVec2(min.x+3,min.y+2),max.x-min.x-6,loc::T("selection.random_pool_skipped"),"skipped-badge");
  }
  if(screen=="roster"&&hasFavourites){
   // The star pins or unpins this card's whole combination.
   const float starSize=22*Scale();
   if(StarButton(ImVec2(max.x-starSize-3*Scale(),min.y+3*Scale()),starSize,isFavourite(rosterCandidate(id))))starId_=e.id;
  }
  return true;
 };
 if(screen=="favourites")card=[&](const MenuEntry& e,ImVec2 min,ImVec2 max){
  if(e.id.compare(0,10,"favourite-")!=0)return false;
  const auto& f=(*favourites->picks)[std::stoi(e.id.substr(10))];
  const float lineHeight=ImGui::GetTextLineHeight();
  const float labelHeight=3*lineHeight+8*Scale();
  ImageInRect(art?art->Appearance(f.fighter,f.costume,f.color):Missing(),ImVec2(min.x+3,min.y+3),ImVec2(max.x-3,max.y-labelHeight));
  auto* d=ImGui::GetWindowDrawList();
  const auto favAvailability=readAvailability?readAvailability(f.fighter):Availability{};
  if(!Available(FavouriteCandidate(f,editionSelect,favAvailability,installed),editionSelect,favAvailability,installed))
   d->AddRectFilled(ImVec2(min.x+3,min.y+3),ImVec2(max.x-3,max.y-labelHeight),IM_COL32(16,15,14,150));
  d->AddRectFilled(ImVec2(min.x,max.y-labelHeight),max,IM_COL32(16,15,14,235));
  // Returns the width it drew, so the edition can be struck through.
  const auto line=[&](float y,const std::string& text,ImU32 color){
   const float natural=(std::max)(1.f,ImGui::CalcTextSize(text.c_str()).x);
   const float font=(std::min)(ImGui::GetFontSize(),(max.x-min.x-6)*ImGui::GetFontSize()/natural);
   d->AddText(ImGui::GetFont(),font,ImVec2(min.x+3,y),color,text.c_str());
   return natural*font/ImGui::GetFontSize();
  };
  const float textTop=max.y-labelHeight+2*Scale();
  line(textTop,e.label,palette::Ivory);
  line(textTop+lineHeight,std::string(UltraLabel(f.ultra))+" · "+loc::T("selection.personal_action")+" "+
   (f.personalAction==255?std::string(loc::T("common.none")):std::to_string(f.personalAction+1)),palette::Muted);
  // The stored edition is always named; with the room's rule off it is struck
  // through, because applying the favourite then drops it.
  const std::string edition=FindEdition(f.edition)->name;
  if(editionSelect)line(textTop+2*lineHeight,edition,palette::Muted);
  else{
   const std::string text=edition+" · "+loc::T("selection.favourite_rule_off");
   const float width=line(textTop+2*lineHeight,text,palette::Muted);
   const float editionWidth=width*ImGui::CalcTextSize(edition.c_str()).x/(std::max)(1.f,ImGui::CalcTextSize(text.c_str()).x);
   const float y=textTop+2*lineHeight+lineHeight*.55f;
   d->AddLine(ImVec2(min.x+3,y),ImVec2(min.x+3+editionWidth,y),palette::Muted);
  }
  const float starSize=22*Scale();
  if(StarButton(ImVec2(max.x-starSize-3*Scale(),min.y+3*Scale()),starSize,true))starId_=e.id;
  return true;
 };
 // What the status line promises depends on the page: Select saves a card on
 // the galleries, opens a page from the menus, and does nothing on the option rows.
 const bool selectSaves=screen=="roster"||screen=="favourites"||screen=="costumes"||screen=="colors"||screen=="ultra"||screen=="stage"||pool;
 const std::string status=!selectionError.empty()?selectionError:!notice_.empty()?notice_:
  loc::T(!editable?"selection.status_locked":screen=="favourites"?"selection.status_favourites":
   screen=="roster"&&hasFavourites&&!KeyboardPrompts()?"selection.status_roster":
   selectSaves?"selection.status_editable":
   screen=="home"||screen=="appearance"?"selection.status_browse":"selection.status_adjust");
 // stableStatus: the galleries must not shift under a highlight when the
 // status grows from one line to two.
 auto a=menu_.Draw(title.c_str(),rows,status.c_str(),preview,columns,card,{},0,
  screen=="costumes"||screen=="colors"?180.f:cardHeight,true,selectionError.empty()?Tone::Neutral:Tone::Error);
 // Delete unpins the focused favourite instead of leaving the seat.
 const bool leaveRemoves=a.kind==MenuAction::Shortcut&&a.delta==static_cast<int>(MenuInput::Leave)&&
  screen=="favourites"&&hasFavourites&&!favourites->picks->empty();
 // RB acts on the focused card: it pins or unpins that combination, opens the
 // page from the favourites card, and goes back from the page. Elsewhere it is
 // consumed so the room's own shortcuts do not see it.
 const bool favouriteShortcut=a.kind==MenuAction::Shortcut&&a.delta==static_cast<int>(MenuInput::Favourite);
 if(a.kind==MenuAction::Close||(a.kind==MenuAction::Shortcut&&!leaveRemoves&&!favouriteShortcut))ForwardMenuAction(a);
 // Opened for one change, Back from its page goes back to where it came from.
 if(!openOn_.empty()&&a.kind==MenuAction::Returned&&screen==openOn_)ForwardMenuAction({MenuAction::Close});
 // A pick is done: back to where fighter select was opened from.
 const auto finish=[&]{if(!openOn_.empty())ForwardMenuAction({MenuAction::Close});else nav.Home();};
 const auto removeFavourite=[&](std::size_t index){
  if(index>=favourites->picks->size())return;
  favourites->picks->erase(favourites->picks->begin()+index);favourites->listChanged=true;
  notice_=loc::T("selection.favourite_removed");
  // Focus what took its place, or the row before it, or the roster door.
  nav.Prefer(favourites->picks->empty()?"roster":
   "favourite-"+std::to_string((std::min)(index,favourites->picks->size()-1)));
 };
 const auto addFavourite=[&](int fighter){
  const auto targetAvailability=readAvailability?readAvailability(fighter):Availability{};
  const auto result=AddFavourite(*favourites->picks,rosterCandidate(fighter),editionSelect,targetAvailability,installed);
  if(result==FavouriteAdd::Added){favourites->listChanged=true;notice_=loc::T("selection.favourite_added");}
  else if(result==FavouriteAdd::Full)notice_=loc::Tf("selection.favourite_full",FavouriteLimit);
  else notice_=loc::T(result==FavouriteAdd::Duplicate?"selection.favourite_duplicate":"runtime.selection_combination_unavailable");
 };
 const auto toggleFavourite=[&](int fighter){
  const auto found=std::find(favourites->picks->begin(),favourites->picks->end(),rosterCandidate(fighter));
  if(found!=favourites->picks->end())removeFavourite(static_cast<std::size_t>(found-favourites->picks->begin()));
  else addFavourite(fighter);
 };
 if(a.kind!=MenuAction::None)notice_.clear();
 // RB never has to reach the grid's footer: it acts on the focused card.
 if(favouriteShortcut){
  if(screen=="favourites")nav.Return();
  else if(screen=="roster"){
   const std::string focus=nav.Focus();
   if(focus=="favourites")nav.Push("favourites");
   else if(focus.compare(0,8,"fighter-")==0)toggleFavourite(std::stoi(focus.substr(8)));
  }
 }
 // A star press is settled on release, when the card's own button fires; that
 // button's action is dropped so only the star counts.
 if(!starId_.empty()&&!ImGui::IsMouseDown(ImGuiMouseButton_Left)&&
  (a.kind==MenuAction::None||a.kind==MenuAction::Activate)){
  if(a.kind==MenuAction::Activate)a={};
  if(screen=="favourites"&&starId_.compare(0,10,"favourite-")==0)removeFavourite(std::stoi(starId_.substr(10)));
  else if(screen=="roster"&&starId_.compare(0,8,"fighter-")==0)toggleFavourite(std::stoi(starId_.substr(8)));
  starId_.clear();
 }
 if(leaveRemoves){
  const std::string focus=nav.Focus();
  if(focus.compare(0,10,"favourite-")==0)removeFavourite(std::stoi(focus.substr(10)));
 }
 if(a.kind==MenuAction::Activate){
  if(screen=="home"||screen=="appearance"||a.id=="random-pool")nav.Push(a.id);
  else if(screen=="favourites"&&a.id=="roster")nav.Push("roster");
  else if(a.id=="favourites")nav.Push("favourites");
  else if(hasFavourites&&a.id=="favourite-add")addFavourite(addFighter_>=0?addFighter_:pick.fighter);
  else if(editable&&screen=="favourites"&&a.id.compare(0,10,"favourite-")==0){
   // Applying is atomic: the entry is fitted and checked before the live pick
   // is written, and the fighter's own memory follows it.
   const auto& favourite=(*favourites->picks)[std::stoi(a.id.substr(10))];
   const auto favAvailability=readAvailability?readAvailability(favourite.fighter):Availability{};
   Pick applied=pick;
   if(ApplyFavourite(favourite,applied,editionSelect,favAvailability,installed)){
    pick=applied;changed=true;favourites->applied=true;
    if(favourites->remember)favourites->remember(pick);
    ForwardMenuAction({MenuAction::Close});
   }else notice_=loc::T("runtime.selection_combination_unavailable");
  }
  else if(editable&&randomPool&&a.id.compare(0,5,"pool-")==0){
   // The page disables the last game stage left; the pool refuses it too.
   if(randomPool->Toggle(std::stoi(a.id.substr(5))))changed=true;
  }
  else if(editable&&a.id.compare(0,8,"fighter-")==0){
   pick.fighter=std::stoi(a.id.substr(8));const auto next=readAvailability?readAvailability(pick.fighter):Availability{};
   Normalize(pick,editionSelect,&next,installed);changed=true;
   // The Ultra comes next, when the fighter has more than one.
   // The Ultra comes next; the next frame drops the step if this fighter,
   // with its own saved pick restored, has only one.
   nav.Push("ultra");nav.Prefer("ultra-"+std::to_string(pick.ultra));focusSavedUltra_=ultraStepPending_=true;
  }else if(editable&&a.id.compare(0,6,"ultra-")==0){pick.ultra=std::stoi(a.id.substr(6));changed=true;finish();}
  else if(editable&&a.id.compare(0,8,"costume-")==0){
   // A costume's colors come next, from the color the pick keeps.
   pick.costume=std::stoi(a.id.substr(8));Normalize(pick,editionSelect,&availability,installed);changed=true;
   nav.Push("colors");nav.Prefer("color-"+std::to_string(pick.color));
  }else if(editable&&a.id.compare(0,6,"color-")==0){pick.color=std::stoi(a.id.substr(6));changed=true;finish();}
  else if(editable&&stageId&&a.id.compare(0,6,"stage-")==0){
   *stageId=std::stoi(a.id.substr(6));changed=true;
   // Opened for this one change, the pick is it; from its own menu the stage page stays.
   if(!openOn_.empty())finish();
  }
 }else if(editable&&a.kind==MenuAction::Adjust){
  if(a.id=="ultra")Step(pick.ultra,AllowedUltras(pick.fighter,pick.edition),a.delta);
  else if(a.id=="costume")Step(pick.costume,AllowedCostumes(pick.fighter,availability,installed),a.delta);
  else if(a.id=="color")Step(pick.color,AllowedColors(pick.fighter,pick.costume,availability,installed),a.delta);
  else if(a.id=="edition")Step(pick.edition,AllowedEditions(pick.fighter,editionSelect),a.delta);
  else if(a.id=="action")Step(pick.personalAction,AllowedPersonalActions(availability),a.delta);
  else if(a.id=="quote")Step(pick.winQuote,{255,0,1,2,3,4,5,6,7,8,9,10},a.delta);
  else if(a.id=="handicap")Step(pick.handicap,{0,1,2,3,4},a.delta);
  Normalize(pick,editionSelect,&availability,installed);changed=true;
 }
 return changed;
}
} }
