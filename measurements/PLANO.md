# Plano de medição: módulos de referência

Para cada linha: ajuste os knobs como indicado (o que não for citado fica no **padrão** do módulo),
exporte o sinal de teste e salve com o nome da coluna "Arquivo".
Posições sem número na escala estão em "horas do relógio" (7h = mínimo, 12h = meio, 17h = máximo).
Se não conseguir a posição exata, use a mais próxima e anote no nome do arquivo.

## AirEQ Air
| Arquivo | Ajuste |
|---|---|
| `air_padrao.wav` | tudo no padrão |
| `air_+6.wav` | Air +6 |
| `air_+12.wav` | Air +12 |
| `air_+18.wav` | Air +18 |
| `air_-12.wav` | Air −12 |
| `air_hicut_10k_s12.wav` | HiCut ligado, Freq 10k, Slope 12, Q padrão |

## AirEQ Earth
| Arquivo | Ajuste |
|---|---|
| `earth_padrao.wav` | tudo no padrão |
| `earth_+6.wav` | Earth +6 |
| `earth_+12.wav` | Earth +12 |
| `earth_+18.wav` | Earth +18 |
| `earth_-12.wav` | Earth −12 |
| `earth_locut_100_s12.wav` | LoCut ligado, Freq 100, Slope 12, Q padrão |

## Trimmer
| Arquivo | Ajuste |
|---|---|
| `trim_0.wav` | Trim 0 |
| `trim_+12.wav` | Trim +12 |
| `trim_-12.wav` | Trim −12 |

## FG-Bomber
| Arquivo | Ajuste |
|---|---|
| `bomber_padrao.wav` | tudo no padrão (Drive 0, Intensity 50%, Tone Fat, Output 0) |
| `bomber_drive+6.wav` | Drive +6 |
| `bomber_drive+15.wav` | Drive +15 |
| `bomber_drive+6_int0.wav` | Drive +6, Intensity 0% |
| `bomber_drive+6_int100.wav` | Drive +6, Intensity 100% |
| `bomber_drive+6_present.wav` | Drive +6, Tone Present |
| `bomber_drive+6_tight.wav` | Drive +6, Tone Tight |

## Revival
| Arquivo | Ajuste |
|---|---|
| `revival_0_0.wav` | Shimmer 0, Thickness 0 |
| `revival_shimmer20.wav` | Shimmer 20, Thickness 0 |
| `revival_shimmer40.wav` | Shimmer 40, Thickness 0 |
| `revival_thick20.wav` | Shimmer 0, Thickness 20 |
| `revival_thick40.wav` | Shimmer 0, Thickness 40 |

## The Monster
| Arquivo | Ajuste |
|---|---|
| `monster_padrao.wav` | tudo no padrão |
| `monster_in15h.wav` | Input em 15h (3 horas) |
| `monster_inmax.wav` | Input no máximo |
| `monster_in15h_a1r1.wav` | Input 15h, Attack 1, Release 1 |
| `monster_in15h_a7r7.wav` | Input 15h, Attack 7, Release 7 |
| `monster_in15h_punch.wav` | Input 15h, Punch ligado |
| `monster_in15h_hpf100.wav` | Input 15h, HPF em ~100 |
| `monster_in15h_mix50.wav` | Input 15h, Mix 50 |

## Depois de exportar
1. Coloque todos os `.wav` numa pasta junto com `mr_measure.py` e `analisar_tudo.bat`
   (da pasta `tools/measure` do repositório) e dê duplo clique no `analisar_tudo.bat`.
2. Envie para esta pasta (`measurements/`) só os arquivos `*_report.json` e `*_report.png`.
