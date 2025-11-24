<div align="center">

<img src="https://capsule-render.vercel.app/api?type=waving&color=auto&height=240&section=header&text=HardStress&fontSize=80&fontColor=ffffff" alt="Banner do HardStress"/>

# HardStress
### Um Toolkit Profissional para Análise de Estabilidade e Desempenho do Sistema.

<p>
    <a href="https://github.com/felipebehling/Hardstress/actions/workflows/build.yml">
        <img src="https://github.com/felipebehling/Hardstress/actions/workflows/build.yml/badge.svg" alt="Build e Release">
    </a>
    <a href="https://opensource.org/licenses/MIT">
        <img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="Licença: MIT">
    </a>
    <a href="https://github.com/felipebehling/Hardstress">
        <img src="https://img.shields.io/badge/platform-linux%20%7C%20windows-blue" alt="Plataforma">
    </a>
</p>

<p align="center">
  <a href="#-sobre-o-projeto">Sobre</a> •
  <a href="#-principais-recursos">Recursos</a> •
  <a href="#-começando">Começando</a> •
  <a href="#-uso">Uso</a> •
  <a href="#-desenvolvimento">Desenvolvimento</a> •
  <a href="#-contribuindo">Contribuindo</a> •
  <a href="#-licença">Licença</a> •
  <a href="#-agradecimentos">Agradecimentos</a>
</p>
</div>

---

## Equipe: Felipe Behling, Gustavo H. Probst, Tiago R. de Melo

## 📖 Sobre o Projeto

O HardStress oferece um método sofisticado e confiável para submeter sistemas computacionais a cargas de trabalho intensas e sustentadas. É um instrumento essencial para analistas de sistemas, engenheiros de hardware e entusiastas de desempenho que precisam validar a estabilidade do sistema, analisar o desempenho térmico e identificar gargalos de desempenho com precisão.

<!-- Placeholder para uma captura de tela ou GIF de alta qualidade da UI em ação -->
<!-- <div align="center">
    <img src="caminho/para/screenshot.png" alt="UI do HardStress" width="700"/>
</div> -->

---

## 🔬 Como Funciona

O HardStress emprega uma abordagem multifacetada para submeter o seu sistema a uma carga intensa e abrangente. Em vez de executar um único tipo de operação repetidamente, ele lança vários threads de trabalho, cada um executando um ciclo de "kérneis" de estresse especializados. Cada kérnel é projetado para atingir um subsistema específico do seu processador e memória:

-   `kernel_fpu`: Satura a **Unidade de Ponto Flutuante (FPU)** com cálculos massivos de multiplicação e adição, testando o desempenho em tarefas matemáticas e científicas.
-   `kernel_int`: Desafia as **Unidades Lógicas e Aritméticas (ALUs)** com operações complexas de inteiros e bitwise, simulando cargas de trabalho de uso geral e lógico.
-   `kernel_stream`: Estressa o **barramento de memória e os controladores** ao realizar transferências de dados em larga escala, identificando gargalos na largura de banda da memória.
-   `kernel_ptrchase`: Testa o **cache da CPU e o prefetcher de memória** criando longas e imprevisíveis cadeias de acesso à memória, medindo a eficiência do sistema em cenários de acesso a dados esparsos.

Essa combinação garante que não apenas os núcleos da CPU, mas todo o subsistema de memória sejam levados aos seus limites, proporcionando um teste de estresse mais realista e revelador.

---

## ✨ Principais Recursos

O HardStress é projetado em torno de três princípios fundamentais: Precisão, Clareza e Controle.

| Recurso     | Descrição                                                                                                                                                                                                                               |
| :---------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **🎯 Precisão** | **Arquitetura Multi-Threaded:** Utiliza eficientemente todos os núcleos de CPU disponíveis, garantindo uma carga de trabalho máxima e sustentada. **Afinidade de CPU:** Permite fixar threads de trabalho a núcleos de CPU específicos. Isso elimina a sobrecarga do escalonador do sistema operacional e garante que a carga em cada núcleo seja consistente e repetível, o que é crucial para testes de benchmark precisos. |
| **📊 Clareza**   | **Visualização em Tempo Real:** A interface gráfica, construída com GTK3, oferece uma visão clara e imediata das principais métricas do sistema. **Gráficos Detalhados:** Monitore o uso de cada núcleo da CPU individualmente, visualize o histórico de desempenho (iterações por segundo) para cada thread e acompanhe as principais métricas térmicas para evitar o superaquecimento. |
| **⚙️ Controle**    | **Parâmetros de Teste Configuráveis:** Ajuste o número de threads, a quantidade de memória alocada por thread e a duração do teste para simular diferentes cenários de carga. Uma duração de `0` permite um teste de estresse contínuo. |

---

## 🚀 Começando

Binários pré-compilados para Linux e Windows estão disponíveis na [seção de Releases](https://github.com/felipebehling/Hardstress/releases).

### Pré-requisitos

<details>
<summary><strong>🐧 Linux (Debian/Ubuntu)</strong></summary>

<br>

Um compilador C e as bibliotecas de desenvolvimento do GTK3 são necessários.
```bash
sudo apt update
sudo apt install build-essential libgtk-3-dev libhpdf-dev git make
```
Para monitoramento térmico, `lm-sensors` é altamente recomendado:
```bash
sudo apt install lm-sensors
```
</details>

<details>
<summary><strong>🪟 Windows (MSYS2)</strong></summary>

<br>

Instale o ambiente [MSYS2](https://www.msys2.org/). No terminal MSYS2 MINGW64, instale a cadeia de ferramentas e as bibliotecas necessárias:
```bash
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-gtk3 mingw-w64-x86_64-libharu pkg-config git make
```
> **Nota para Usuários do Windows:** O Windows Defender SmartScreen pode sinalizar o executável pré-compilado, pois ele não é assinado digitalmente. A aplicação é segura e seu código-fonte está aberto para auditoria. Para executá-lo, clique em "Mais informações" no aviso do SmartScreen e, em seguida, em "Executar assim mesmo". Além disso, para que as métricas de desempenho (como o uso da CPU) apareçam corretamente, pode ser necessário executar a aplicação com privilégios de administrador. Clique com o botão direito em `HardStress.exe` e selecione 'Executar como administrador'.
</details>

<details>
<summary><strong>🪟 Windows (WSL)</strong></summary>

<br>

Instale o [Subsistema do Windows para Linux (WSL)](https://learn.microsoft.com/pt-br/windows/wsl/install) e uma distribuição Linux (por exemplo, Ubuntu) da Microsoft Store. No seu terminal WSL, instale as dependências:
```bash
sudo apt update
sudo apt install build-essential libgtk-3-dev libhpdf-dev git make
```
> **Nota para Usuários do WSL:** Para executar aplicações GUI no WSL, você precisará do WSLg, que está incluído no Windows 11 e em versões mais recentes do WSL para Windows 10. Certifique-se de que seu sistema está atualizado.
</details>

---

## 👨‍💻 Uso

1.  **Configure os Parâmetros do Teste:**
    -   **Threads:** Defina o número de threads de trabalho.
    -   **Mem (MiB/thread):** Especifique a quantidade de RAM a ser alocada por cada thread.
    -   **Duração (s):** Defina a duração do teste. Use `0` para uma execução indefinida.
    -   **Fixar threads nas CPUs:** Habilite a afinidade de CPU para máxima consistência do teste.
2.  **Inicie o Teste:** Clique em `Iniciar`.
3.  **Monitore o Desempenho:** Observe as visualizações de dados em tempo real.
4.  **Conclua o Teste:** Clique em `Parar` para encerrar o teste manualmente.
5.  **Limpar Log:** Clique em `Limpar Log` para limpar o log de eventos.

---

## 🛠️ Desenvolvimento

Para compilar o projeto a partir do código-fonte, clone o repositório e use o Makefile incluído.

```bash
git clone https://github.com/felipebehling/Hardstress.git
cd Hardstress
```

**Compile a aplicação:**
-   Para uma compilação de depuração padrão: `make`
-   Para uma compilação de lançamento de alto desempenho: `make release`

**Execute a suíte de testes:**
-   `make test`

Este comando compila e executa uma suíte de testes unitários para validar as funções principais de utilidade e métricas.

---

## 🤝 Contribuindo

As contribuições são o que tornam a comunidade de código aberto um lugar incrível para aprender, inspirar e criar. Qualquer contribuição que você fizer será **muito apreciada**.

Se você tiver uma sugestão que possa melhorar este projeto, por favor, faça um fork do repositório e crie um pull request. Você também pode simplesmente abrir uma issue com a tag "enhancement".
Não se esqueça de dar uma estrela ao projeto! Obrigado novamente!

1. Faça um Fork do Projeto
2. Crie sua Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Faça Commit de suas Mudanças (`git commit -m 'Add some AmazingFeature'`)
4. Faça Push para a Branch (`git push origin feature/AmazingFeature`)
5. Abra um Pull Request

---

## 📜 Licença

Este projeto está licenciado sob a Licença MIT. Consulte o arquivo [LICENSE](LICENSE) para obter detalhes.

---

## 🙏 Agradecimentos

Um agradecimento especial aos seguintes projetos e comunidades por sua inspiração e pelas ferramentas que tornaram este projeto possível:

-   [Shields.io](https://shields.io/) pelos emblemas dinâmicos.
-   [Capsule Render](https://github.com/kyechan99/capsule-render) pelo incrível banner de cabeçalho.
-   A comunidade de código aberto por fornecer recursos e suporte incríveis.

---

---

## 💻 Pilha Tecnológica

Este projeto foi construído com as seguintes tecnologias e padrões:

-   **Linguagem Principal:** C (padrões C99 e C11)
-   **Interface Gráfica:** GTK3
-   **Sistema de Build:** Make
-   **Controle de Versão:** Git
-   **Compiladores:** GCC (Linux) e MinGW-w64 (Windows)

<p align="center">
  <em>Um toolkit profissional para análise de estabilidade e desempenho do sistema.</em>
</p>
