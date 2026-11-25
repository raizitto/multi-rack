# Medir um plugin de referência ("caixa-preta")

Este guia mostra como medir o comportamento sonoro de um plugin que você tem instalado
(por exemplo, um módulo do Virtual Mix Rack) para ajustar o módulo equivalente do Multi Rack.
Nada é descompilado: só passamos um sinal de teste pelo plugin e comparamos a entrada com a saída.

## O que o sinal de teste mede

`tools/measure/test_signal_48k.wav` (55 s, mono, 48 kHz, 32-bit float) contém:

| Trecho | Para quê |
|---|---|
| Varredura 20 Hz–20 kHz a −18 dBFS | Curva de EQ/tonalidade em nível baixo |
| Varredura 20 Hz–20 kHz a −6 dBFS | Curva de EQ em nível alto (mostra se a saturação muda o timbre) |
| Senoides 1 kHz, 100 Hz e 5 kHz de −48 a +6 dBFS (passos de 3 dB) | Curva de saturação/compressão, ganho e harmônicos (H2, H3...) em cada nível |
| Degraus de nível −30 → −6 → −30 dBFS em 1 kHz | Tempo de ataque e de release de compressores |

## Passo a passo no FL Studio

1. **Baixe o sinal de teste:**
   https://github.com/raizitto/multi-rack/raw/main/tools/measure/test_signal_48k.wav
2. Em *Options → Audio settings*, deixe a taxa de amostragem em **48000 Hz** (recomendado).
3. Arraste o WAV para a Playlist, no **início do compasso 1**. Envie o canal para um Mixer Insert.
4. No Insert, coloque **apenas** o módulo que você quer medir. Deixe o fader do Insert e do Master em **0 dB** e sem nada no Master.
5. Ajuste os knobs do módulo na configuração que quer medir. Anote os valores ou tire um print.
6. *File → Export → Wave file*:
   - Formato: **32-bit float**
   - Sem *Normalize*, sem *Dither*
   - *Tail: Cut remainder*
   - Qualidade de resampling: qualquer
7. Dê um nome que descreva a configuração, por exemplo:
   `vchannel_drive5_input0.wav`, `fgstress_input20_ratio4_atk3_rel5.wav`.
8. Repita os passos 5–7 para cada configuração que quiser comparar.
   Vale começar por: tudo no padrão, drive/input no meio, drive/input no máximo.

Dicas:
- Plugins com ruído simulado (hiss/hum) devem ter o ruído **desligado** para a medição.
- Se o plugin tiver oversampling/HQ, meça com a mesma opção que você usa normalmente e anote qual foi.
- Não precisa exportar o sinal "seco": o analisador gera a referência sozinho.

## Como me enviar

No GitHub, abra a pasta [`measurements/`](../measurements) → **Add file → Upload files**,
arraste os WAVs (cada um tem ~21 MB, dentro do limite de 25 MB do upload pelo site) e clique em **Commit changes**.
Junto, escreva num arquivo `.txt` (ou na mensagem do commit) qual plugin/módulo e quais valores de knobs cada arquivo usou.

Com os arquivos no repositório, eu rodo a análise, comparo com o módulo equivalente do Multi Rack e ajusto o DSP.

## Rodar a análise você mesmo (opcional)

Requer Python 3.9+:

```bash
pip install numpy scipy matplotlib
```

```bash
python tools/measure/mr_measure.py analyze vchannel_drive5_input0.wav
```

```bash
python tools/measure/mr_measure.py compare referencia.wav multirack.wav
```

O `analyze` gera um `.json` com todos os números e um `.png` com seis gráficos
(resposta de frequência em dois níveis, curva de transferência, harmônicos a 100 Hz e 1 kHz, ganho no tempo).
O `compare` sobrepõe duas medições no mesmo gráfico.

Para renderizar o sinal pelo próprio Multi Rack (compilando o harness de testes):

```bash
./build-test/MultiRackTest_artefacts/Release/MultiRackTest --render tools/measure/test_signal_48k.wav saida.wav channel 1=5 2=1
```

(`channel` = id do módulo; `1=5` = parâmetro 1 (Drive) em 5; `2=1` = parâmetro 2 (Character) na opção 1.)
