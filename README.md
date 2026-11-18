# dodeligQ-1

Ecualizador paramétrico VST3 de 8 bandas para Windows 11 y Reaper.
Filtros: campana, estante grave, estante agudo, paso alto, paso bajo y muesca.
Analizador de espectro en tiempo real.

## Controles
- Arrastra un nodo de color: cambia frecuencia (horizontal) y ganancia (vertical).
- Rueda del ratón sobre el gráfico: cambia la Q de la banda seleccionada.
- Botones 1–8: seleccionan banda. "Activa" la habilita o deshabilita.

## Compilar en GitHub (sin instalar nada)
1. Sube esta carpeta a un repositorio nuevo en tu cuenta.
2. Ve a la pestaña **Actions** y espera a que termine "Build dodeligQ-1 VST3".
3. Descarga el artefacto **dodeligQ-1-vst3** (un .vst3).

## Instalar
Copia `dodeligQ-1.vst3` en `C:\Program Files\Common Files\VST3\`.
En Reaper: Opciones > Preferencias > Plug-ins > VST, pulsa "Re-scan" y añade el plugin a una pista.

## Compilar en local (opcional)
Requiere Visual Studio 2022 con C++ y CMake:
    cmake -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release --target dodeligQ1_VST3
