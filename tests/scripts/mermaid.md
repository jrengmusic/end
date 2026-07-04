# Mermaid Rendering Test

---

## Flowchart

```mermaid
graph TD
    A[Start] --> B{Decision}
    B -->|Yes| C[Action 1]
    B -->|No| D[Action 2]
    C --> E[End]
    D --> E
```

---

## Sequence Diagram

```mermaid
sequenceDiagram
    participant A as Client
    participant B as Server
    A->>B: Request
    B-->>A: Response
    A->>B: Acknowledge
```

---

## Class Diagram

```mermaid
classDiagram
    class Component {
        +paint()
        +resized()
    }
    class Screen {
        +load()
        +build()
        +updateLayout()
    }
    Component --> Screen
```

---

## State Diagram

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Parsing : openFile
    Parsing --> Building : parse complete
    Building --> Ready : all blocks built
    Ready --> Idle : close
```

---

## Gantt Chart

```mermaid
gantt
    title Whelmed Phases
    section Phase 1
    CPU Rendering : done, p1, 2026-03-01, 30d
    section Phase 2
    GPU Rendering : p2, after p1, 30d
    section Phase 3
    Hybrid : p3, after p2, 30d
```

---

## Pie Chart

```mermaid
pie title Block Types
    "TextBlock" : 150
    "CodeBlock" : 30
    "MermaidBlock" : 8
    "TableBlock" : 6
```

---

## Simple Flowchart LR

```mermaid
graph LR
    A --> B --> C --> D
```

---

## End
