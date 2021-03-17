
#define FZY_SCORE_MAX INFINITY
#define FZY_SCORE_MIN -INFINITY
#define FZY_SCORE_GAP_LEADING -0.005
#define FZY_SCORE_GAP_TRAILING -0.005
#define FZY_SCORE_GAP_INNER -0.01
#define FZY_SCORE_MATCH_CONSECUTIVE 1.0
#define FZY_SCORE_MATCH_SLASH 0.9
#define FZY_SCORE_MATCH_WORD 0.8
#define FZY_SCORE_MATCH_CAPITAL 0.7
#define FZY_SCORE_MATCH_DOT 0.6

#define FZY_SCORE_EPSILON 0.001

using fzy_score_t = f64;

static fzy_score_t fzy_bonus_states[3][256];
static size_t fzy_bonus_index[256];


static void fzy_init_table()
{
    memset(fzy_bonus_index, 0, sizeof fzy_bonus_index);
    memset(fzy_bonus_states, 0, sizeof fzy_bonus_states);

    for (i32 i = 'A'; i < 'Z'; i++) {
        fzy_bonus_index[i] = 2;
    }

    for (i32 i = 'a'; i < 'z'; i++) {
        fzy_bonus_index[i] = 1;
        fzy_bonus_states[2][i] = FZY_SCORE_MATCH_CAPITAL;
    }

    for (i32 i = '0'; i < '9'; i++) {
        fzy_bonus_index[i] = 1;
    }

    fzy_bonus_states[1]['/'] = fzy_bonus_states[2]['/'] = FZY_SCORE_MATCH_SLASH;
    fzy_bonus_states[1]['\\'] = fzy_bonus_states[2]['\\'] = FZY_SCORE_MATCH_SLASH;
    fzy_bonus_states[1]['-'] = fzy_bonus_states[2]['-'] = FZY_SCORE_MATCH_WORD;
    fzy_bonus_states[1]['_'] = fzy_bonus_states[2]['_'] = FZY_SCORE_MATCH_WORD;
    fzy_bonus_states[1][' '] = fzy_bonus_states[2][' '] = FZY_SCORE_MATCH_WORD;
    fzy_bonus_states[1]['.'] = fzy_bonus_states[2]['.'] = FZY_SCORE_MATCH_DOT;
}

#define fzy_compute_bonus(last_ch, ch) \
    (fzy_bonus_states[fzy_bonus_index[(unsigned char)(ch)]][(unsigned char)(last_ch)])

static void quicksort(fzy_score_t *scores, Lister_Node **nodes, i32 l, i32 r)
{
    if (l >= r) {
        return;
    }

    fzy_score_t pivot = scores[r];

    i32 cnt = l;

    for (i32 i = l; i <= r; i++) {
        if (scores[i] >= pivot) {
            swap(nodes[cnt], nodes[i]);
            swap(scores[cnt], scores[i]);
            cnt++;
        }
    }

    quicksort(scores, nodes, l, cnt-2);
    quicksort(scores, nodes, cnt, r);
}

