inputs = []

for i in range(5):
  with open('_' + str(i) + '.txt', 'r') as f:
    inputs.append(f.readlines())

output = [min(lines) for lines in zip(*inputs, strict=True)]

with open('results.txt', 'a') as f:
  f.writelines(output)
