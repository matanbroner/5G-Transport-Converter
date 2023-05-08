import random
import string

def random_iteration_id_string():
    """
    Generate a random iteration ID string
    """
    return ''.join(random.choice(string.ascii_lowercase) for i in range(10))