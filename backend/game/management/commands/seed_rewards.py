from django.core.management.base import BaseCommand
from game.models import Reward


class Command(BaseCommand):
    help = 'Seeds the database with initial rewards'

    def handle(self, *args, **options):
        rewards = [
            # Daubers (cute shapes for marking numbers)
            {
                'name': 'Heart Dauber',
                'description': 'A sweet pink heart to mark your numbers!',
                'reward_type': 'dauber',
                'reward_value': 'heart',
                'coin_cost': 0,
                'icon': 'heart',
            },
            {
                'name': 'Star Dauber',
                'description': 'Sparkly golden stars!',
                'reward_type': 'dauber',
                'reward_value': 'star',
                'coin_cost': 150,
                'icon': 'star',
            },
            {
                'name': 'Paw Print Dauber',
                'description': 'Adorable kitty paw prints!',
                'reward_type': 'dauber',
                'reward_value': 'paw',
                'coin_cost': 200,
                'icon': 'paw',
            },
            {
                'name': 'Flower Dauber',
                'description': 'Beautiful blooming flowers!',
                'reward_type': 'dauber',
                'reward_value': 'flower',
                'coin_cost': 250,
                'icon': 'flower',
            },
            {
                'name': 'Rainbow Dauber',
                'description': 'Magical rainbow swirls!',
                'reward_type': 'dauber',
                'reward_value': 'rainbow',
                'coin_cost': 400,
                'gem_cost': 2,
                'icon': 'rainbow',
            },
            {
                'name': 'Crown Dauber',
                'description': 'For the bingo royalty!',
                'reward_type': 'dauber',
                'reward_value': 'crown',
                'streak_requirement': 5,
                'coin_cost': 500,
                'icon': 'crown',
            },

            # Themes (background colors/styles)
            {
                'name': 'Pastel Dreams',
                'description': 'Soft pastel pink and lavender theme',
                'reward_type': 'theme',
                'reward_value': 'pastel',
                'coin_cost': 300,
                'icon': 'palette',
            },
            {
                'name': 'Ocean Breeze',
                'description': 'Calming ocean blues and teals',
                'reward_type': 'theme',
                'reward_value': 'ocean',
                'coin_cost': 300,
                'icon': 'water',
            },
            {
                'name': 'Cherry Blossom',
                'description': 'Beautiful sakura pink theme',
                'reward_type': 'theme',
                'reward_value': 'sakura',
                'coin_cost': 450,
                'gem_cost': 1,
                'icon': 'flower',
            },
            {
                'name': 'Golden Sunset',
                'description': 'Warm sunset oranges and pinks',
                'reward_type': 'theme',
                'reward_value': 'sunset',
                'coin_cost': 500,
                'gem_cost': 2,
                'icon': 'sun',
            },
            {
                'name': 'Magical Night',
                'description': 'Mystical purple with sparkles!',
                'reward_type': 'theme',
                'reward_value': 'night',
                'streak_requirement': 3,
                'coin_cost': 350,
                'icon': 'moon',
            },
            {
                'name': 'Rainbow Paradise',
                'description': 'All the colors of the rainbow!',
                'reward_type': 'theme',
                'reward_value': 'rainbow',
                'streak_requirement': 10,
                'coin_cost': 1000,
                'gem_cost': 5,
                'is_premium': True,
                'icon': 'rainbow',
            },
            {
                'name': 'Aurora Borealis',
                'description': 'Dancing northern lights in the sky',
                'reward_type': 'theme',
                'reward_value': 'aurora',
                'coin_cost': 400,
                'gem_cost': 1,
                'icon': 'sparkles',
            },
            {
                'name': 'Cotton Candy',
                'description': 'Sweet swirls of pink and blue',
                'reward_type': 'theme',
                'reward_value': 'candy',
                'coin_cost': 250,
                'icon': 'candy',
            },
            {
                'name': 'Lavender Fields',
                'description': 'Peaceful purple flower meadows',
                'reward_type': 'theme',
                'reward_value': 'lavender',
                'coin_cost': 300,
                'icon': 'flower',
            },
            {
                'name': 'Mint Fresh',
                'description': 'Cool and refreshing mint green',
                'reward_type': 'theme',
                'reward_value': 'mint',
                'coin_cost': 200,
                'icon': 'leaf',
            },
            {
                'name': 'Rose Garden',
                'description': 'Romantic roses in bloom',
                'reward_type': 'theme',
                'reward_value': 'rose',
                'coin_cost': 350,
                'icon': 'rose',
            },
            {
                'name': 'Cosmic Galaxy',
                'description': 'Deep space nebulas and stars',
                'reward_type': 'theme',
                'reward_value': 'galaxy',
                'streak_requirement': 5,
                'coin_cost': 600,
                'gem_cost': 3,
                'icon': 'star',
            },
            {
                'name': 'Peach Blossom',
                'description': 'Soft peachy warmth and blooms',
                'reward_type': 'theme',
                'reward_value': 'peach',
                'coin_cost': 275,
                'icon': 'peach',
            },
            {
                'name': 'Butterfly Garden',
                'description': 'Magical butterflies fluttering by',
                'reward_type': 'theme',
                'reward_value': 'butterfly',
                'coin_cost': 450,
                'gem_cost': 2,
                'icon': 'butterfly',
            },
            {
                'name': 'Coral Reef',
                'description': 'Vibrant underwater coral paradise',
                'reward_type': 'theme',
                'reward_value': 'coral',
                'coin_cost': 400,
                'gem_cost': 1,
                'icon': 'fish',
            },
            {
                'name': 'Strawberry Dream',
                'description': 'Sweet strawberry fields forever',
                'reward_type': 'theme',
                'reward_value': 'strawberry',
                'coin_cost': 300,
                'icon': 'strawberry',
            },
            {
                'name': 'Enchanted Forest',
                'description': 'Mystical woodland magic',
                'reward_type': 'theme',
                'reward_value': 'forest',
                'streak_requirement': 7,
                'coin_cost': 550,
                'gem_cost': 2,
                'icon': 'tree',
            },
            {
                'name': 'Sunny Farm',
                'description': 'Adorable baby animals on a sunny farm',
                'reward_type': 'theme',
                'reward_value': 'farm',
                'coin_cost': 350,
                'gem_cost': 1,
                'icon': 'cow',
            },

            # Card Backs
            {
                'name': 'Bunny Pattern',
                'description': 'Cute bunnies hopping around!',
                'reward_type': 'card_back',
                'reward_value': 'bunny',
                'coin_cost': 200,
                'icon': 'rabbit',
            },
            {
                'name': 'Kitten Dreams',
                'description': 'Sleeping kittens pattern',
                'reward_type': 'card_back',
                'reward_value': 'kitten',
                'coin_cost': 200,
                'icon': 'cat',
            },
            {
                'name': 'Puppy Love',
                'description': 'Playful puppies everywhere!',
                'reward_type': 'card_back',
                'reward_value': 'puppy',
                'coin_cost': 200,
                'icon': 'dog',
            },
            {
                'name': 'Sparkle Magic',
                'description': 'Glittery sparkle pattern',
                'reward_type': 'card_back',
                'reward_value': 'sparkle',
                'coin_cost': 350,
                'gem_cost': 1,
                'icon': 'sparkles',
            },
            {
                'name': 'Royal Velvet',
                'description': 'Luxurious velvet pattern',
                'reward_type': 'card_back',
                'reward_value': 'velvet',
                'streak_requirement': 7,
                'coin_cost': 600,
                'gem_cost': 3,
                'is_premium': True,
                'icon': 'crown',
            },
        ]

        created_count = 0
        for reward_data in rewards:
            reward, created = Reward.objects.get_or_create(
                name=reward_data['name'],
                defaults=reward_data
            )
            if created:
                created_count += 1
                self.stdout.write(f"Created reward: {reward.name}")

        self.stdout.write(
            self.style.SUCCESS(f'Successfully created {created_count} rewards!')
        )
