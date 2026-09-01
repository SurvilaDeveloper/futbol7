# Futbol7 — Etapa 8F — Auditoría de atributos conectados al gameplay

Esta auditoría corresponde al cierre del bloque 8 de `SoccerPlayerProfile`.

## Regla de compatibilidad

Un actor sin `SoccerPlayerProfile` conserva el comportamiento legacy de cada sistema. Los multiplicadores y ajustes de atributos sólo se aplican cuando existe un perfil válido.

## Atributos físicos

| Atributo | Estado | Gameplay conectado |
|---|---|---|
| Pace | Conectado | Velocidades Walk/Jog/Run/FastRun. |
| Acceleration | Conectado | `MaxAcceleration`. |
| Stamina | Conectado | Ritmo de consumo de energía. |
| StaminaRecovery | Conectado | Ritmo de recuperación de energía. |
| Strength | Conectado en 8F | Fuerza/resistencia en choques aéreos existentes y desplazamiento/inercia tras caída por tackle. También aporta de forma secundaria en una disputa aérea. |
| Agility | Conectado | Duración física de giros de dribbling/autopase. |
| Balance | Conectado | Retención de velocidad después de cambios bruscos de dirección. |

Nota sobre `Strength`: el proyecto no tiene todavía un sistema general de forcejeo hombro-a-hombro para jugadores simplemente corriendo uno al lado del otro. Por eso 8F no inventa una nueva colisión; Strength se integra sólo en consecuencias físicas que ya existen.

## Atributos técnicos

| Atributo | Estado | Gameplay conectado |
|---|---|---|
| BallControl | Conectado | Consistencia/distancia de toques y degradación bajo presión. |
| Dribbling | Conectado | Precisión direccional de conducción y autopase. |
| PassingAccuracy | Conectado | Error direccional y variación de fuerza de pases. |
| ShootingAccuracy | Conectado | Dispersión de remates. |
| ShotPower | Conectado | Potencia efectiva de remate. |
| Tackling | Conectado | Envolvente física de contacto con pelota y decisión de tackle de IA. |
| AerialAbility | Conectado en 8F | Calidad/contacto aéreo, margen físico moderado de contacto de cabeza, disputa aérea, potencia de cabezazos y error de ejecución de cabezazos de IA. |

## Atributos tácticos/mentales

| Atributo | Estado | Gameplay conectado |
|---|---|---|
| DefensiveReaction | Conectado | Tiempo/frecuencia de reacción defensiva de IA. |
| Anticipation | Conectado | Confianza/horizonte predictivo defensivo y lectura ofensiva. |
| OffBallPositioning | Conectado | Error respecto de posiciones ofensivas ideales. |
| DefensivePositioning | Conectado | Error respecto de posiciones defensivas ideales. |
| Marking | Conectado | Precisión/actualización del marcaje. |
| DecisionMaking | Conectado | Demora y umbrales de decisiones ofensivas de IA. |
| Composure | Conectado | Calidad técnica y decisión bajo presión. |

## Atributos de arquero

| Atributo | Estado | Gameplay conectado |
|---|---|---|
| Reflexes | Conectado | Horizonte y demora de inicio de reacción. |
| Positioning | Conectado | Error lateral/profundidad respecto de posición ideal. |
| Handling | Conectado | Margen efectivo de captura de manos. |
| Diving | Conectado | Alcance adaptativo lateral de la atajada. |
| Distribution | Conectado | Precisión y potencia de distribución. |

## Etapa 8F — detalle

### Strength

- No usa una probabilidad oculta de “ganar choque”.
- En un choque aéreo, el impulso recibido depende de la fuerza del que empuja y de la resistencia del que lo recibe.
- Aporta sólo una ventaja secundaria al `ContestScore`; la calidad real de llegada/contacto sigue siendo dominante.
- Un jugador fuerte conserva mejor su posición física después de una caída por tackle al reducir el desplazamiento inercial posterior. No evita automáticamente la falta ni la animación de caída.

### AerialAbility

- Modifica de forma moderada el radio efectivo de contacto de cabeza, sin cambiar la geometría del cuerpo ni estirar el esqueleto.
- Modifica la calidad del contacto: timing, posición, orientación y velocidad siguen siendo necesarios; el atributo no crea un cabezazo exitoso por sí solo.
- Es la capacidad principal en el `ContestScore` de una pelota aérea; Strength es secundaria.
- Modifica la potencia de cabezazos activos y defensivos.
- En IA reduce/aumenta la dispersión ya existente de cabezazos de pase/remate.
- En la coordinación de IA ayuda a elegir, entre candidatos de llegada similar, al jugador más competente por arriba.

## Ajustes globales nuevos

En Class Defaults del personaje:

- `Soccer → Player Profile → Physical Contest Tuning`
- `Soccer → Player Profile → Aerial Tuning`

En Class Defaults del `SoccerAIController`:

- `Soccer → Player Profile → Aerial Tuning`

Los valores de los Data Assets siguen siendo únicamente ratings 0–100. Los parámetros anteriores definen globalmente qué significa cada extremo de la escala.

## Cierre del bloque 8

Con 8F, todos los atributos 0–100 actualmente definidos en `SoccerPlayerProfile` tienen una conexión concreta con gameplay. Esto no significa que su balance esté finalizado: la siguiente fase recomendable es probar perfiles extremos y después ajustar los parámetros globales de tuning sin reescribir los Data Assets.
