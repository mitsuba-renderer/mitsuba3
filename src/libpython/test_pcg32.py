import unittest
from mitsuba import pcg32


class PCG32Test(unittest.TestCase):
    values_32bit = [0xa15c02b7, 0x7b47f409, 0xba1d3330, 0x83d2f293,
                    0xbfa4784b, 0xcbed606e]
    values_coins = 'HHTTTHTHHHTHTTTHHHHHTTTHHHTHTHTHTTHTTTHHHHHHTT' \
                   'TTHHTTTTTHTTTTTTTHT'
    values_rolls = [3, 4, 1, 1, 2, 2, 3, 2, 4, 3, 2, 4, 3, 3, 5, 2, 3, 1, 3,
                    1, 5, 1, 4, 1, 5, 6, 4, 6, 6, 2, 6, 3, 3]
    values_cards = 'Qd Ks 6d 3s 3d 4c 3h Td Kc 5c Jh Kd Jd As 4s 4h Ad Th ' \
                   'Ac Jc 7s Qs 2s 7h Kh 2d 6c Ah 4d Qh 9h 6s 5s 2c 9c Ts ' \
                   '8d 9s 3c 8c Js 5d 2h 6h 7d 8s 9d 5h 8h Qc 7c Tc'

    def test01_seed(self):
        p = pcg32()
        p.seed(42, 54)
        base = pcg32(p)

        for v in PCG32Test.values_32bit:
            self.assertEqual(p.nextUInt(), v)

        p.advance(-4)
        self.assertEqual(p - base, 2)

        for v in PCG32Test.values_32bit[2:]:
            self.assertEqual(p.nextUInt(), v)

        for v in PCG32Test.values_coins:
            self.assertEqual('H' if p.nextUInt(2) == 1 else 'T', v)

        for v in PCG32Test.values_rolls:
            self.assertEqual(p.nextUInt(6) + 1, v)

        cards = list(range(52))
        p.shuffle(cards)

        number = ['A', '2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J',
                  'Q', 'K']
        suit = ['h', 'c', 'd', 's']
        ctr = 0

        for v in PCG32Test.values_cards.split():
            self.assertEqual(number[cards[ctr] // len(suit)] +
                             suit[cards[ctr] % len(suit)], v)
            ctr += 1

if __name__ == '__main__':
    unittest.main()
