#include "worker.hpp"
#include <elo.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <libataxx/pgn.hpp>
#include <mutex>
#include <sprt.hpp>
#include "play.hpp"
#include "results.hpp"
#include "settings.hpp"

std::mutex mtx_output;
std::mutex mtx_games;

void print_score(const Settings &settings,
                 const EngineSettings &engine1,
                 const EngineSettings &engine2,
                 const Results &results,
                 const bool show_elo = true) {
    const auto w = results.scores.at(engine1.name).wins;
    const auto l = results.scores.at(engine1.name).losses;
    const auto d = results.scores.at(engine1.name).draws;

    std::cout << "Score of ";
    std::cout << engine1.name << " vs " << engine2.name;
    std::cout << ": " << results.scores.at(engine1.name);
    std::cout << " " << results.scores.at(engine1.name).played;
    std::cout << "\n";
    if (show_elo) {
        std::cout << std::fixed << std::setprecision(2) << get_elo(w, l, d) << " +/- " << get_err(w, l, d) << "\n";

        if (settings.sprt_enabled && settings.engines.size() == 2) {
            const auto llr = sprt::get_llr(w, l, d, settings.sprt_elo0, settings.sprt_elo1);
            const auto lbound = sprt::get_lbound(settings.sprt_alpha, settings.sprt_beta);
            const auto ubound = sprt::get_ubound(settings.sprt_alpha, settings.sprt_beta);
            std::cout << "SPRT: llr " << llr << ", lbound " << lbound << ", ubound " << ubound << "\n";
        }
    }
}

void worker(const Settings &settings, std::queue<GameSettings> &games, Results &results) {
    while (true) {
        GameSettings game;

        // Get something to do
        {
            std::lock_guard<std::mutex> lock(mtx_games);
            if (games.empty()) {
                return;
            }
            game = games.front();
            games.pop();
        }

        if (settings.verbose) {
            std::lock_guard<std::mutex> lock(mtx_output);
            std::cout << "Starting game " << game.engine1.name << " vs " << game.engine2.name << "\n";
        }

        libataxx::pgn::PGN pgn;

        // Play the game
        try {
            pgn = play(settings, game);
        } catch (std::invalid_argument &e) {
            std::cerr << e.what() << "\n";
        } catch (const char *e) {
            std::cerr << e << "\n";
        } catch (std::exception &e) {
            std::cerr << e.what() << "\n";
        } catch (...) {
            std::cerr << "Error woops\n";
            continue;
        }

        auto should_stop = false;

        // Results & printing
        {
            std::lock_guard<std::mutex> lock(mtx_output);

            if (settings.verbose) {
                std::cout << "Finishing game " << game.engine1.name << " vs " << game.engine2.name << "\n";
            }

            results.games_played++;

            // Update engine results
            results.scores[game.engine1.name].played++;
            results.scores[game.engine2.name].played++;
            if (pgn.header().get("Result") == "1-0") {
                results.scores[game.engine1.name].wins++;
                results.scores[game.engine2.name].losses++;
                results.black_wins++;
            } else if (pgn.header().get("Result") == "0-1") {
                results.scores[game.engine1.name].losses++;
                results.scores[game.engine2.name].wins++;
                results.white_wins++;
            } else if (pgn.header().get("Result") == "1/2-1/2") {
                results.scores[game.engine1.name].draws++;
                results.scores[game.engine2.name].draws++;
                results.draws++;
            }

            // Write to .pgn
            if (settings.pgn_enabled && !settings.pgn_path.empty()) {
                std::ofstream file(settings.pgn_path, std::fstream::out | std::fstream::app);

                if (file.is_open()) {
                    file << pgn;
                } else {
                    std::cerr << "Could not open " << settings.pgn_path << "\n";
                }
            }

            const auto w = results.scores.at(game.engine1.name).wins;
            const auto l = results.scores.at(game.engine1.name).losses;
            const auto d = results.scores.at(game.engine1.name).draws;
            const auto llr = sprt::get_llr(w, l, d, settings.sprt_elo0, settings.sprt_elo1);
            const auto lbound = sprt::get_lbound(settings.sprt_alpha, settings.sprt_beta);
            const auto ubound = sprt::get_ubound(settings.sprt_alpha, settings.sprt_beta);
            const auto sprt_stop = settings.sprt_enabled && settings.sprt_autostop && (llr <= lbound || llr >= ubound);

            should_stop |= sprt_stop;

            // Print results
            if (results.scores.size() == 2) {
                const auto print_result = results.games_played < settings.ratinginterval ||
                                          results.games_played % settings.ratinginterval == 0 ||
                                          results.games_played == results.games_total || sprt_stop;
                const auto show_elo = results.games_played >= settings.ratinginterval ||
                                      settings.num_games == results.games_played || sprt_stop;

                if (print_result) {
                    if (game.engine1.id < game.engine2.id) {
                        print_score(settings, game.engine1, game.engine2, results, show_elo);
                    } else {
                        print_score(settings, game.engine2, game.engine1, results, show_elo);
                    }

                    if (show_elo) {
                        std::cout << "\n";
                    }
                }
            } else if (results.games_played % settings.ratinginterval == 0 ||
                       results.games_played == results.games_total) {
                for (const auto &[name, score] : results.scores) {
                    std::cout << name << ": ";
                    std::cout << score;
                    std::cout << " " << score.played;
                    std::cout << "\n";
                }
                std::cout << "\n";
            }
        }

        if (should_stop) {
            std::lock_guard<std::mutex> lock(mtx_games);
            games = {};
        }
    }
}
