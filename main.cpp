#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <imgui-SFML.h>
#include <imgui_stdlib.h>
#include "Widgets.h"
#include "EditorState.h"
#include "tinyfiledialogs.h"
#include "Toolbox.h"

int main(int argc, char** argv) {

    // TODO : Highlight des notes qui s'entrechoquent
    // TODO : scroll to modifed note when undoing/redoing
    // TODO : Debug Log
    // TODO : Bruit différent si clap simple ou chord
    // TODO : Density graph sur la timeline
    // TODO : Système de notifs
    // TODO : Pitch control (playback speed factor)
    // TODO : A small preference save system (marker , ...)

    // Création de la fenêtre
    sf::RenderWindow window(sf::VideoMode(800, 600), "FEIS");
    sf::RenderWindow & ref_window = window;
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(60);

    ImGui::SFML::Init(window, false);

    ImGuiIO& IO = ImGui::GetIO();
    IO.Fonts->Clear();
    IO.Fonts->AddFontFromFileTTF("assets/fonts/NotoSans-Medium.ttf", 16.f);
    ImGui::SFML::UpdateFontTexture();

    std::string noteTickPath = "assets/sounds/sound note tick.wav";
    sf::SoundBuffer noteTick;
    if (!noteTick.loadFromFile(noteTickPath)) {
        std::cerr << "Unable to load sound " << noteTickPath;
        throw std::runtime_error("Unable to load sound " + noteTickPath);
    }
    sf::Sound noteTickSound(noteTick);
    int noteTickVolume = 10;
    bool playNoteTick = false;
    int lastTickTicked = -1;

    std::string beatTickPath = "assets/sounds/sound beat tick.wav";
    sf::SoundBuffer beatTick;
    if (!beatTick.loadFromFile(beatTickPath)) {
        std::cerr << "Unable to load sound " << beatTickPath;
        throw std::runtime_error("Unable to load sound " + beatTickPath);
    }
    sf::Sound beatTickSound(beatTick);
    int beatTickVolume = 10;
    bool playBeatTick = false;
    bool beatTicked = false;


    // Loading markers preview
    std::map<std::filesystem::path,sf::Texture> markerPreviews;
    for (const auto& folder : std::filesystem::directory_iterator("assets/textures/markers")) {
        if (folder.is_directory()) {
            sf::Texture markerPreview;
            markerPreview.loadFromFile((folder/"ma15.png").string());
            markerPreview.setSmooth(true);
            markerPreviews.insert({folder,markerPreview});
        }
    }

    Marker defaultMarker;
    Marker& marker = defaultMarker;
    MarkerEndingState markerEndingState = MarkerEndingState_MISS;

    Widgets::Ecran_attente bg;
    std::optional<EditorState> editorState;
    ESHelper::NewChartDialog newChartDialog;
    ESHelper::ChartPropertiesDialog chartPropertiesDialog;

    sf::Clock deltaClock;
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(event);

            switch (event.type) {
                case sf::Event::Closed:
                    window.close();
                    break;
                case sf::Event::Resized:
                    window.setView(sf::View(sf::FloatRect(0, 0, event.size.width, event.size.height)));
                    break;
                case sf::Event::KeyPressed:
                    switch (event.key.code) {
                        case sf::Keyboard::Up:
                            if (event.key.shift) {
                                if (editorState) {
                                    if (editorState->musicVolume < 10) {
                                        editorState->musicVolume++;
                                        editorState->updateMusicVolume();
                                    }
                                }
                            } else {
                                if (editorState and editorState->chart) {
                                    float floatTicks = editorState->getTicks();
                                    int prevTick = static_cast<int>(floorf(floatTicks));
                                    int step = editorState->getSnapStep();
                                    int prevTickInSnap = prevTick;
                                    if (prevTick%step == 0) {
                                        prevTickInSnap -= step;
                                    } else {
                                        prevTickInSnap -= prevTick%step;
                                    }
                                    editorState->setPlaybackAndMusicPosition(sf::seconds(editorState->getSecondsAt(prevTickInSnap)));
                                }
                            }
                            break;
                        case sf::Keyboard::Down:
                            if (event.key.shift) {
                                if (editorState) {
                                    if (editorState->musicVolume > 0) {
                                        editorState->musicVolume--;
                                        editorState->updateMusicVolume();
                                    }
                                }
                            } else {
                                if (editorState and editorState->chart) {
                                    float floatTicks = editorState->getTicks();
                                    int nextTick = static_cast<int>(ceilf(floatTicks));
                                    int step = editorState->getSnapStep();
                                    int nextTickInSnap = nextTick + (step - nextTick%step);
                                    editorState->setPlaybackAndMusicPosition(sf::seconds(editorState->getSecondsAt(nextTickInSnap)));
                                }
                            }
                            break;
                        case sf::Keyboard::Left:
                            if (editorState and editorState->chart) {
                                editorState->snap = Toolbox::getPreviousDivisor(editorState->chart->ref.getResolution(),editorState->snap);
                            }
                            break;
                        case sf::Keyboard::Right:
                            if (editorState and editorState->chart) {
                                editorState->snap = Toolbox::getNextDivisor(editorState->chart->ref.getResolution(),editorState->snap);
                            }
                            break;
                        case sf::Keyboard::F3:
                            playBeatTick = not playBeatTick;
                            break;
                        case sf::Keyboard::F4:
                            playNoteTick = not playNoteTick;
                            break;
                        case sf::Keyboard::Space:
                            if (not ImGui::GetIO().WantTextInput) {
                                if (editorState) {
                                    editorState->playing = not editorState->playing;
                                }
                            }
                            break;
                        case sf::Keyboard::O:
                            if (event.key.control) {
                                ESHelper::open(editorState);
                            }
                            break;
                        case sf::Keyboard::P:
                            if (event.key.shift) {
                                editorState->showProperties = true;
                            }
                            break;
                        case sf::Keyboard::S:
                            if (event.key.control) {
                                ESHelper::save(*editorState);
                            }
                            break;
                        case sf::Keyboard::Y:
                            if (event.key.control) {
                                if (editorState and editorState->chart) {
                                    auto next = editorState->chart->history.get_next();
                                    if (next) {
                                        (*next)->doAction(*editorState);
                                    }
                                }
                            }
                            break;
                        case sf::Keyboard::Z:
                            if (event.key.control) {
                                if (editorState and editorState->chart) {
                                    auto previous = editorState->chart->history.get_previous();
                                    if (previous) {
                                        (*previous)->undoAction(*editorState);
                                    }
                                }
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
        }

        sf::Time delta = deltaClock.restart();
        ImGui::SFML::Update(window, delta);

        // Audio playback management
        if (editorState) {
            editorState->updateVisibleNotes();
            if (editorState->playing) {
                editorState->previousPos = editorState->playbackPosition;
                editorState->playbackPosition += delta;
                if (editorState->music) {
                    switch(editorState->music->getStatus()) {
                        case sf::Music::Stopped:
                        case sf::Music::Paused:
                            if (editorState->playbackPosition.asSeconds() >= 0 and editorState->playbackPosition < editorState->music->getDuration()) {
                                editorState->music->setPlayingOffset(editorState->playbackPosition);
                                editorState->music->play();
                            }
                            break;
                        case sf::Music::Playing:
                            editorState->playbackPosition = editorState->music->getPlayingOffset();
                            break;
                        default:
                            break;
                    }
                }
                if (playBeatTick) {
                    if (fmodf(editorState->getBeats(),1.f) < 0.5) {
                        if (not beatTicked) {
                            beatTickSound.play();
                            beatTicked = true;
                        }
                    } else {
                        beatTicked = false;
                    }
                }
                if (playNoteTick) {
                    for (auto note : editorState->visibleNotes) {
                        float noteTiming = editorState->getSecondsAt(note.getTiming());
                        if (noteTiming >= editorState->previousPos.asSeconds()
                            and noteTiming <= editorState->playbackPosition.asSeconds()
                            and note.getTiming() > editorState->lastTimingTicked) {
                            noteTickSound.play();
                            editorState->lastTimingTicked = note.getTiming();
                        }
                    }
                } else {
                    editorState->lastTimingTicked = -1;
                }
                if (editorState->playbackPosition >= editorState->chartRuntime) {
                    editorState->playing = false;
                    editorState->playbackPosition = editorState->chartRuntime;
                }
            } else {
                if (editorState->music) {
                    if (editorState->music->getStatus() == sf::Music::Playing) {
                        editorState->music->pause();
                    }
                }
            }
        }

        // Drawing
        if (editorState) {

            window.clear(sf::Color(0, 0, 0));

            if (editorState->showHistory) {
                editorState->chart->history.display(print_history_message);
            }
            if (editorState->showPlayfield) {
                editorState->displayPlayfield(marker,markerEndingState);
            }
            if (editorState->showProperties) {
                editorState->displayProperties();
            }
            if (editorState->showStatus) {
                editorState->displayStatus();
                ImGui::Begin("Status");
                {
                    ImGui::Checkbox("Beat Tick",&playBeatTick); ImGui::SameLine();
                    if (ImGui::SliderInt("Beat Tick Volume",&beatTickVolume,0,10)) {
                        Toolbox::updateVolume(beatTickSound,beatTickVolume);
                    }
                    ImGui::Checkbox("Note Tick",&playNoteTick); ImGui::SameLine();
                    if (ImGui::SliderInt("Note Tick Volume",&noteTickVolume,0,10)) {
                        Toolbox::updateVolume(noteTickSound,noteTickVolume);
                    }
                }
                ImGui::End();
            }
            if (editorState->showPlaybackStatus) {
                editorState->displayPlaybackStatus();
            }
            if (editorState->showTimeline) {
                editorState->displayTimeline();
            }
            if (editorState->showChartList) {
                editorState->displayChartList();
            }
            if (editorState->showNewChartDialog) {
                std::optional<Chart> c = newChartDialog.display(*editorState);
                if (c) {
                    editorState->showNewChartDialog = false;
                    if(editorState->fumen.Charts.try_emplace(c->dif_name,*c).second) {
                        editorState->chart->ref = editorState->fumen.Charts[c->dif_name];
                    }
                }
            } else {
                newChartDialog.resetValues();
            }
            if (editorState->showChartProperties) {
                chartPropertiesDialog.display(*editorState);
            } else {
                chartPropertiesDialog.shouldRefreshValues = true;
            }
        } else {
            bg.render(window);
        }

        // Dessin de l'interface
        ImGui::BeginMainMenuBar();
        {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New")) {
                    const char* _filepath = tinyfd_saveFileDialog("New File",nullptr,0,nullptr,nullptr);
                    if (_filepath != nullptr) {
                        std::filesystem::path filepath(_filepath);
                        try {
                            Fumen f(filepath);
                            f.autoSaveAsMemon();
                            editorState.emplace(f);
                            Toolbox::pushNewRecentFile(std::filesystem::canonical(editorState->fumen.path));
                        } catch (const std::exception& e) {
                            tinyfd_messageBox("Error",e.what(),"ok","error",1);
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Open","Ctrl+O")) {
                    ESHelper::open(editorState);
                }
                if (ImGui::BeginMenu("Recent Files")) {
                    int i = 0;
                    for (const auto& file : Toolbox::getRecentFiles()) {
                        ImGui::PushID(i);
                        if (ImGui::MenuItem(file.c_str())) {
                            ESHelper::openFromFile(editorState,file);
                        }
                        ImGui::PopID();
                        ++i;
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Close","",false,editorState.has_value())) {
                    editorState.reset();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save","Ctrl+S",false,editorState.has_value())) {
                    ESHelper::save(*editorState);
                }
                if (ImGui::MenuItem("Save As","",false,editorState.has_value())) {
                    char const * options[1] = {"*.memon"};
                    const char* _filepath(tinyfd_saveFileDialog("Save File",nullptr,1,options,nullptr));
                    if (_filepath != nullptr) {
                        std::filesystem::path filepath(_filepath);
                        try {
                            editorState->fumen.saveAsMemon(filepath);
                        } catch (const std::exception& e) {
                            tinyfd_messageBox("Error",e.what(),"ok","error",1);
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Properties","Shift+P",false,editorState.has_value())) {
                    editorState->showProperties = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Chart",editorState.has_value())) {
                if (ImGui::MenuItem("Chart List")) {
                    editorState->showChartList = true;
                }
                if (ImGui::MenuItem("Properties##Chart",nullptr,false,editorState->chart.has_value())) {
                    editorState->showChartProperties = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("New Chart")) {
                    editorState->showNewChartDialog = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete Chart",nullptr,false,editorState->chart.has_value())) {
                    editorState->fumen.Charts.erase(editorState->chart->ref.dif_name);
                    editorState->chart.reset();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View",editorState.has_value())) {
                if (ImGui::MenuItem("Playfield", nullptr,editorState->showPlayfield)) {
                    editorState->showPlayfield = not editorState->showPlayfield;
                }
                if (ImGui::MenuItem("Playback Status",nullptr,editorState->showPlaybackStatus)) {
                    editorState->showPlaybackStatus = not editorState->showPlaybackStatus;
                }
                if (ImGui::MenuItem("Timeline",nullptr,editorState->showTimeline)) {
                    editorState->showTimeline = not editorState->showTimeline;
                }
                if (ImGui::MenuItem("Editor Status",nullptr,editorState->showStatus)) {
                    editorState->showStatus = not editorState->showStatus;
                }
                if (ImGui::MenuItem("History",nullptr,editorState->showHistory)) {
                    editorState->showHistory = not editorState->showHistory;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Settings",editorState.has_value())) {
                if (ImGui::BeginMenu("Marker")) {
                    int i = 0;
                    for (auto& tuple : markerPreviews) {
                        ImGui::PushID(tuple.first.c_str());
                        if (ImGui::ImageButton(tuple.second,{100,100})) {
                            try {
                                marker = Marker(tuple.first);
                            } catch (const std::exception& e) {
                                tinyfd_messageBox("Error",e.what(),"ok","error",1);
                                marker = defaultMarker;
                            }
                        }
                        ImGui::PopID();
                        i++;
                        if (i%4 != 0) {
                            ImGui::SameLine();
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Marker Ending State")) {
                    for (auto& m : Markers::markerStatePreviews) {
                        if (ImGui::ImageButton(marker.getTextures().at(m.textureName),{100,100})) {
                            markerEndingState = m.state;
                        }
                        ImGui::SameLine();
                        ImGui::TextUnformatted(m.printName.c_str());
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
        }
        ImGui::EndMainMenuBar();

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}