import mpmath


def generate_pi_mpmath():
    mpmath.mp.dps = 1000  # 设置精度为 1000 位
    pi = mpmath.pi
    return pi


def write_pi_to_file(filename):
    pi_value = generate_pi_mpmath()
    with open(filename, 'w') as file:
        file.write(str(pi_value))


if __name__ == "__main__":
    write_pi_to_file('pi.bin')
