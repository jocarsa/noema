# Noema

Lenguaje de programación de estilo **pythonico** (basado en indentación y legibilidad), con un **léxico inspirado en raíces greco‑latinas**, diseñado para ser **internacional, expresivo y minimalista**.

Este documento describe el **núcleo del lenguaje**: instrucciones, palabras reservadas y su uso.

---

## 1. Principios generales

* Sintaxis basada en **indentación** (no se usan `{}` ni `;`).
* Todas las palabras reservadas están en **minúsculas**.
* No se usan acentos ni caracteres especiales.
* El estado se representa mediante **asignación directa** (no hay palabra clave para declarar variables).
* El lenguaje distingue claramente entre **núcleo** y **librería estándar**.

---

## 2. Literales básicos

| Literal  | Significado       |
| -------- | ----------------- |
| `verum`  | Verdadero         |
| `falsum` | Falso             |
| `nulla`  | Ausencia de valor |

---

## 3. Asignación y variables

Noema no requiere declaración explícita de variables. Una variable se crea al asignarle un valor.

```noema
x = 10
nomen = "Marcus"
valor = nulla
```

Las variables son **mutables por defecto**.

---

## 4. Operadores lógicos

| Operador | Significado |
| -------- | ----------- |
| `et`     | AND lógico  |
| `aut`    | OR lógico   |
| `non`    | NOT lógico  |

Ejemplo:

```noema
si x > 0 et x < 10:
    sonus.dic("en rango")
```

---

## 5. Comparadores

Noema utiliza comparadores estándar:

| Operador | Significado   |
| -------- | ------------- |
| `==`     | Igualdad      |
| `!=`     | Desigualdad   |
| `<`      | Menor que     |
| `<=`     | Menor o igual |
| `>`      | Mayor que     |
| `>=`     | Mayor o igual |

---

## 6. Estructuras condicionales

### `si`

Evalúa una condición.

```noema
si edad >= 18:
    sonus.dic("adultus")
```

### `aliosi`

Condición alternativa encadenada (equivalente a `elif`).

```noema
si nota >= 9:
    sonus.dic("excellentia")
aliosi nota >= 5:
    sonus.dic("aprobatus")
```

### `alio`

Bloque alternativo final.

```noema
si valor == nulla:
    sonus.dic("vacuum")
alio:
    sonus.dic("definitum")
```

---

## 7. Bucles

### `pro`

Iteración sobre una secuencia.

```noema
pro i in series(0, 5):
    sonus.dic(i)
```

### `dum`

Bucle condicionado.

```noema
dum x < 10:
    x = x + 1
```

### `frange`

Interrumpe un bucle.

```noema
pro i in series(0, 10):
    si i == 5:
        frange
```

### `perge`

Continúa con la siguiente iteración.

```noema
pro i in series(0, 5):
    si i == 2:
        perge
    sonus.dic(i)
```

---

## 8. Funciones

### `munus`

Define una función.

```noema
munus saluta(nomen):
    redit "salve " + nomen
```

### `redit`

Devuelve un valor desde una función.

```noema
munus quadratum(x):
    redit x * x
```

---

## 9. Excepciones

### `conare`

Bloque de intento.

### `nisi`

Captura una excepción.

### `denique`

Bloque que se ejecuta siempre.

```noema
conare:
    x = divide(10, 0)
nisi:
    sonus.dic("error")
denique:
    sonus.dic("fin")
```

### `iacta`

Lanza una excepción.

```noema
iacta Error("valor invalidus")
```

---

## 10. Módulos e importación

### `import`

Importa un módulo.

```noema
import sonus
```

`import` es una excepción en inglés por ser un estándar internacional consolidado.

---

## 11. Librería estándar (núcleo conceptual)

### `sonus` — Entrada y salida

| Función           | Descripción          |
| ----------------- | -------------------- |
| `sonus.dic(x)`    | Muestra salida       |
| `sonus.lege(msg)` | Lee entrada de texto |

### `series` — Generador de secuencias

```noema
series(inicio, fin)
```

Genera una secuencia iterable desde `inicio` hasta `fin - 1`.

---

## 12. Programa mínimo

```noema
import sonus

munus init():
    sonus.dic("salve mundus")
```

---

## 13. Resumen del núcleo

**Palabras reservadas:**

`si, aliosi, alio, pro, dum, frange, perge, munus, redit, conare, nisi, denique, iacta, import`

**Literales:**

`verum, falsum, nulla`

**Operadores lógicos:**

`et, aut, non`

---

Este documento define el **núcleo estable de Noema**. Todo lo no descrito aquí pertenece a librerías o extensiones del lenguaje.

