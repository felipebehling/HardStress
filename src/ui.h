#ifndef UI_H
#define UI_H

/**
 * @file ui.h
 * @brief Declara funções relacionadas à Interface Gráfica do Usuário baseada em GTK.
 *
 * Este módulo é responsável por criar, gerenciar e atualizar todos os elementos da GUI,
 * incluindo a janela principal, gráficos, campos de entrada e log de eventos.
 * Também lida com interações do usuário e aciona a lógica principal da aplicação.
 */

#include "hardstress.h"

/**
 * @brief Cria e inicializa a janela principal do GTK e todos os seus widgets.
 *
 * Esta função constrói toda a interface do usuário, conectando sinais para
 * botões e outros elementos interativos aos seus respectivos manipuladores de callback.
 *
 * @param app Um ponteiro para a estrutura global `AppContext`.
 * @return Um ponteiro para a `GtkWindow` recém-criada.
 */
GtkWidget* create_main_window(AppContext *app);

/**
 * @brief Registra uma mensagem formatada no painel de log de eventos da GUI.
 *
 * Esta função é segura para threads e pode ser chamada de qualquer thread para anexar
 * uma mensagem com carimbo de data/hora à visualização de log na UI.
 *
 * @param app Um ponteiro para a estrutura global `AppContext`.
 * @param fmt A string de formato no estilo `printf` para a mensagem.
 * @param ... Argumentos variáveis para a string de formato.
 */
void gui_log(AppContext *app, const char *fmt, ...);

/**
 * @brief Uma GSourceFunc para atualizar a GUI após a parada de um teste.
 *
 * Esta função é chamada via `g_idle_add` da thread controladora assim que um teste
 * é concluído. Ela reabilita os controles de configuração e atualiza o rótulo de status.
 *
 * @param ud Um ponteiro para a estrutura global `AppContext`.
 * @return G_SOURCE_REMOVE para garantir que a função seja chamada apenas uma vez.
 */
gboolean gui_update_stopped(gpointer ud);

/**
 * @brief Thread-safe UI update to show or hide the temperature monitor panel.
 * @param app The application context.
 * @param visible TRUE to show the panel, FALSE to hide it.
 */
void gui_set_temp_panel_visibility(AppContext *app, gboolean visible);


#endif // UI_H
