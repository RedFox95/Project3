import random
import string

def generate_file(size, filename):
    with open(filename, 'w') as f:
        for _ in range(size):
            line = ''.join(random.choices(string.ascii_uppercase, k=size))
            f.write(line + '\n')

sizes = {
    'small.txt': 10,
    'medium.txt': 100,
    'large.txt': 1000,
    'extreme.txt': 5000
}

for filename, size in sizes.items():
    generate_file(size, filename)
    print(f'Generated {filename} with size {size}x{size}')
