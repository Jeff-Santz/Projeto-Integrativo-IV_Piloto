EggLink - Sistema de Monitoramento Climático IoT
Projeto Integrativo IV - Engenharia Elétrica (Poli-USP)
Este repositório centraliza o código-fonte desenvolvido para o Sistema de Monitoramento Ambiental e Prevenção de Incêndios, concebido no âmbito da disciplina Projeto Integrativo IV (2024), integrante do Percurso Competências (Piloto) da Escola Politécnica da USP.

O projeto consiste na implementação de uma rede mesh de baixo consumo energético baseada no protocolo OpenThread, utilizando microcontroladores ESP32-C6 para o monitoramento em tempo real de variáveis microclimáticas, como temperatura, umidade do ar, umidade do solo e qualidade do ar.

Equipe de Desenvolvimento
O sistema foi arquitetado e desenvolvido pelos discentes do Percurso Competências:

Isadora Ribeiro Vital

Janos Biezok Neto

Jefferson Santos Monteiro

Jorge Ricardo Barbosa França

Estrutura do Repositório
Para fins de organização, manutenção e avaliação, o projeto foi estruturado como um monorepo, dividido em três diretórios principais que correspondem aos módulos funcionais do sistema:

1. ot_cli_gateway_final (Border Router)
Este diretório contém o firmware desenvolvido para o nó central (Gateway). O código é responsável por gerenciar a coexistência dos rádios Wi-Fi e IEEE 802.15.4, atuando como ponte entre a rede mesh local e a internet. Suas principais funções incluem o recebimento de pacotes via protocolo CoAP e o encaminhamento dos dados processados para o servidor externo através de requisições HTTP POST.

2. ot_cli_nos_final (Sensor Nodes)
Este diretório armazena o firmware replicável destinado aos nós sensores distribuídos. O software gerencia a leitura dos periféricos — incluindo o sensor analógico de gases MQ-135, a adaptação do sensor TDS para umidade do solo e o sensor digital AHT25 via I2C — e executa as rotinas de economia de energia (Deep Sleep) e transmissão de dados via rede Thread.

3. egglink_server (Backend & Dashboard)
Este diretório contém a aplicação completa do servidor web. O backend foi desenvolvido em Python utilizando o framework Flask, sendo responsável pela recepção dos dados do Gateway, lógica de detecção de alertas de incêndio e integração com a API do Google Sheets para persistência histórica. O diretório também inclui os arquivos de frontend do Dashboard para visualização e geolocalização dos dispositivos.

Tecnologias e Ferramentas
Hardware: Espressif ESP32-C6 (RISC-V)

Firmware: ESP-IDF v5.x

Protocolos: OpenThread (IEEE 802.15.4), CoAP, UDP, Wi-Fi, HTTP

Software: C/C++, Python (Flask), Google Apps Script

Créditos e Referências de Terceiros
O desenvolvimento deste projeto utilizou como base bibliotecas e exemplos da comunidade open-source. Especificamente, a implementação do driver de comunicação I2C para os sensores de temperatura e umidade (AHT25) foi adaptada a partir do repositório espidf_beginer, de autoria de pangcrd (https://github.com/pangcrd/espidf_beginer). O código original recebeu modificações para adequação ao sistema operacional de tempo real (FreeRTOS) utilizado neste projeto.

Nota sobre Versionamento
Este repositório consolida módulos que foram desenvolvidos em ambientes separados durante a fase de prototipagem. Arquivos temporários de compilação, binários e dependências locais foram excluídos via .gitignore para assegurar a integridade e limpeza do código-fonte.
