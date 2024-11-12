#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>

using namespace std;
// Global variables for synchronization
constexpr int NUM_JOGADORES = 4;
std::counting_semaphore<NUM_JOGADORES> cadeira_sem(NUM_JOGADORES - 1); // Inicia com n-1 cadeiras
std::condition_variable music_cv;
std::mutex music_mutex;
std::atomic<bool> musica_parada{false};
std::atomic<bool> jogo_ativo{true};
/* 
 * JogoDasCadeiras: Controla a lógica do jogo, como iniciar uma rodada, parar a música e eliminar jogadores.
 */
class JogoDasCadeiras {
public:
    JogoDasCadeiras(int num_jogadores)
        : num_jogadores(num_jogadores), cadeiras(num_jogadores - 1) {}
    void iniciar_rodada() {
        std::unique_lock<std::mutex> lock(music_mutex);
        musica_parada = false;
        cadeiras--;  // Remove uma cadeira a cada rodada
        cadeira_sem = std::counting_semaphore<NUM_JOGADORES>(cadeiras);
        std::cout << "Nova rodada iniciada com " << cadeiras << " cadeiras." << std::endl;
    }
    void parar_musica() {
        std::unique_lock<std::mutex> lock(music_mutex);
        musica_parada = true;
        music_cv.notify_all();  // Notifica todos os jogadores para tentar ocupar uma cadeira
        std::cout << "Música parou! Corram para as cadeiras!" << std::endl;
    }
    void eliminar_jogador(int jogador_id) {
        std::cout << "Jogador " << jogador_id << " foi eliminado." << std::endl;
        num_jogadores--;
        if (num_jogadores == 1) {
            jogo_ativo = false;  // Termina o jogo quando resta apenas um jogador
        }
    }
    void exibir_estado() {
        std::cout << "Jogadores restantes: " << num_jogadores << ", Cadeiras restantes: " << cadeiras << std::endl;
    }
    bool is_jogo_ativo() const { return jogo_ativo; }
    bool is_musica_parada() const { return musica_parada; }
private:
    int num_jogadores;
    int cadeiras;
};
/* 
 * Jogador: Cada jogador tenta ocupar uma cadeira quando a música para.
 */
class Jogador {
public:
    Jogador(int id, JogoDasCadeiras& jogo)
        : id(id), jogo(jogo) {}
    void tentar_ocupar_cadeira() {
        if (jogo.is_musica_parada()) {
            // Tenta adquirir uma cadeira
            if (cadeira_sem.try_acquire()) {
                std::cout << "Jogador " << id << " conseguiu uma cadeira!" << std::endl;
            } else {
                jogo.eliminar_jogador(id);  // Jogador não conseguiu uma cadeira e é eliminado
            }
        }
    }
    void joga() {
        while (jogo.is_jogo_ativo()) {
            // Aguarda até a música parar
            std::unique_lock<std::mutex> lock(music_mutex);
            music_cv.wait(lock, [&] { return jogo.is_musica_parada() || !jogo.is_jogo_ativo(); });
            if (!jogo.is_jogo_ativo()) break;
            // Tenta ocupar uma cadeira
            tentar_ocupar_cadeira();
        }
    }
private:
    int id;
    JogoDasCadeiras& jogo;
};

/* 
 * Coordenador: Controla o ritmo do jogo, parando a música em intervalos aleatórios.
 */
class Coordenador {
public:
    Coordenador(JogoDasCadeiras& jogo)
        : jogo(jogo) {}
    void iniciar_jogo() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(1000, 3000); // Intervalo de 1 a 3 segundos
        while (jogo.is_jogo_ativo()) {
            // Espera um tempo aleatório antes de parar a música
            std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
            jogo.parar_musica();
            // Espera um pouco para dar tempo dos jogadores tentarem sentar
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            liberar_threads_eliminadas(); // Libera threads dos jogadores que ficaram sem cadeira
            if (jogo.is_jogo_ativo()) {
                jogo.iniciar_rodada(); // Inicia a próxima rodada
            }
        }
        std::cout << "Jogo das Cadeiras terminou. Temos um vencedor!" << std::endl;
    }
    void liberar_threads_eliminadas() {
        cadeira_sem.release(NUM_JOGADORES - 1); // Libera o número de permissões igual ao número de jogadores que ficaram esperando
    }
private:
    JogoDasCadeiras& jogo;
};

// Função principal
int main() {
    JogoDasCadeiras jogo(NUM_JOGADORES);
    Coordenador coordenador(jogo);
    std::vector<std::thread> jogadores;
    // Criação das threads dos jogadores
    std::vector<Jogador> jogadores_objs;
    for (int i = 1; i <= NUM_JOGADORES; ++i) {
        jogadores_objs.emplace_back(i, jogo);
    }
    for (int i = 0; i < NUM_JOGADORES; ++i) {
        jogadores.emplace_back(&Jogador::joga, &jogadores_objs[i]);
    }
    // Thread do coordenador
    std::thread coordenador_thread(&Coordenador::iniciar_jogo, &coordenador);
    // Esperar pelas threads dos jogadores
    for (auto& t : jogadores) {
        if (t.joinable()) {
            t.join();
        }
    }
    // Esperar pela thread do coordenador
    if (coordenador_thread.joinable()) {
        coordenador_thread.join();
    }
    std::cout << "Jogo das Cadeiras finalizado." << std::endl;
    return 0;
}
