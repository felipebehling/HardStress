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

## 📖 Sobre o Projeto

O HardStress oferece um método sofisticado e confiável para submeter sistemas computacionais a cargas de trabalho intensas e sustentadas. É um instrumento essencial para analistas de sistemas, engenheiros de hardware e entusiastas de desempenho que precisam validar a estabilidade do sistema, analisar o desempenho térmico e identificar gargalos de desempenho com precisão.

<!-- Placeholder para uma captura de tela ou GIF de alta qualidade da UI em ação -->
<!-- <div align="center">
    <img src="caminho/para/screenshot.png" alt="UI do HardStress" width="700"/>
</div> -->

---

## ✨ Principais Recursos

O HardStress é projetado em torno de três princípios fundamentais: Precisão, Clareza e Controle.

| Recurso     | Descrição                                                                                                                                                                                                                               |
| :---------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **🎯 Precisão** | Emprega uma arquitetura multi-threaded para saturar os núcleos do processador com uma mistura balanceada de operações de ponto flutuante, inteiros e streaming de memória. Oferece a capacidade de fixar threads de trabalho em núcleos de CPU específicos para máxima consistência do teste. |
| **📊 Clareza**   | Apresenta uma visão em tempo real e de alta fidelidade do desempenho da sua máquina através de uma interface gráfica limpa e intuitiva. Fornece visualizações dinâmicas para utilização por núcleo, histórico de desempenho por thread e métricas térmicas críticas. |
| **⚙️ Controle**    | Fornece os controles necessários para configurar os parâmetros do teste de acordo com suas necessidades específicas, incluindo o número de threads, alocação de memória por thread e duração do teste. Todos os dados de séries temporais podem ser exportados para um arquivo CSV para análise aprofundada. |

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
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-gtk3 mingw-w64-x86_64-libharu pkg-config
```
> **Nota para Usuários do Windows:** O Windows Defender SmartScreen pode sinalizar o executável pré-compilado, pois ele não é assinado digitalmente. A aplicação é segura e seu código-fonte está aberto para auditoria. Para executá-lo, clique em "Mais informações" no aviso do SmartScreen e, em seguida, em "Executar assim mesmo". Além disso, para que as métricas de desempenho (como o uso da CPU) apareçam corretamente, pode ser necessário executar a aplicação com privilégios de administrador. Clique com o botão direito em `HardStress.exe` e selecione 'Executar como administrador'.
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
5.  **Exporte os Resultados:** Após a conclusão do teste, clique em `Exportar CSV` para salvar os dados de desempenho.

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

<p align="center">
  <em>Um toolkit profissional para análise de estabilidade e desempenho do sistema.</em>
</p>
