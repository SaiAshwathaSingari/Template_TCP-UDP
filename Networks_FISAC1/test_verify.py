import hashlib
import base64

tests = [
    ("uKAkRguXbyw1QbM7WHraVA==", "rDxxioYc3a9nKgRnfOAM4wd0Ja8="),
    ("ETWBev9L+vzy0CLP4k98KA==", "NCtVqdOsJIfYe5B84kxSL/CrFbU="),
    ("giyOykUltn0Lmf+n10oxgg==", "zzexEJMvnXJHBJrmbxcCZDtyONU="),
]

magic = "258EAFA5-E914-47DA-95CA-5AB4AA29BE5E"
all_ok = True
for key, server_accept in tests:
    expected = base64.b64encode(hashlib.sha1((key + magic).encode()).digest()).decode()
    ok = (expected == server_accept)
    print(f"Key: {key}")
    print(f"  Expected: {expected}")
    print(f"  Server:   {server_accept}")
    print(f"  Match: {ok}")
    if not ok:
        all_ok = False

print(f"\nAll Accept keys correct: {all_ok}")
