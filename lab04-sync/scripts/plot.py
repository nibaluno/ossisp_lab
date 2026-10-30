import csv, collections
import matplotlib.pyplot as plt

rows = list(csv.DictReader(open("../data/results.csv")))
g = collections.defaultdict(list)
for r in rows:
    g[r["label"]].append(r)
avg = lambda lab, key: sum(float(x[key]) for x in g[lab]) / len(g[lab])

sw = sorted((l for l in g if l.startswith("sweep_W")), key=lambda l: int(l[7:]))
ws = [int(l[7:]) for l in sw]

plt.figure()
plt.plot(ws, [avg(l, "total_ms") for l in sw], "o-")
plt.xlabel("Число писателей (3 читателя)"); plt.ylabel("Время выполнения, мс")
plt.title("Время выполнения от числа писателей"); plt.grid(True)
plt.savefig("../data/graph1_time.png", dpi=150)

plt.figure()
plt.plot(ws, [avg(l, "avg_wait_r") for l in sw], "o-", label="Читатели")
plt.plot(ws, [avg(l, "avg_wait_w") for l in sw], "s-", label="Писатели")
plt.xlabel("Число писателей (3 читателя)"); plt.ylabel("Среднее время ожидания, мс")
plt.title("Время ожидания по категориям"); plt.legend(); plt.grid(True)
plt.savefig("../data/graph2_wait.png", dpi=150)