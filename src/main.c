
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    HC_CELL_NONE,
    HC_CELL_VERT,
    HC_CELL_HORZ,
    HC_CELL_U_R,
    HC_CELL_D_R,
    HC_CELL_D_L,
    HC_CELL_U_L
} HilbertCell_t;

typedef enum {
    HC_DIR_RIGHT,
    HC_DIR_DOWN,
    HC_DIR_LEFT,
    HC_DIR_UP,
    HC_NUM_DIRS,
} HilbertDir_t;

typedef struct {
    int            order;
    int            sidelen; // Length of each side of the square, array is ^2 in size
    HilbertCell_t *cells;
} HilbertGraph_t;

const char *HC_ALGO_PROD_A = "-BF+AFA+FB-";
const char *HC_ALGO_PROD_B = "+AF-BFB-FA+";

typedef enum {
    HC_SYM_NULL    = 0,
    HC_SYM_SUB_A   = 'A',
    HC_SYM_SUB_B   = 'B',
    HC_SYM_FORWARD = 'F',
    HC_SYM_LEFT    = '-',
    HC_SYM_RIGHT   = '+'
} HilbertProdSym_t;

typedef struct {
    const char      *prod;
    int              prod_index;
} HilbertAlgoRecurse_t;

typedef struct {
    HilbertGraph_t       *graph;
    HilbertDir_t          currDir;
    HilbertDir_t          prevDir;
    HilbertAlgoRecurse_t *state;
    int                   state_index;
    int                   x;
    int                   y;
} HilbertAlgoContext_t;

// Parameters

static int pOrder = 6; // Hilbert Curve order
static int pScale = 2; // 4x4 squares, scaled by pScale in each dimension

static const int pMaxW  = 512;
static const int pMaxH  = 512;

// Runtime Variables

static bool            sSDLInited;
static bool            sIMGInited;
static SDL_Window     *sWindow;
static SDL_Renderer   *sRenderer;

static SDL_Surface    *sSpriteSurface;
static SDL_Texture    *sSpriteTexture;

static SDL_Texture    *sScreenTexture;

static uint32_t winW, winH; // Calculated based on pOrder, pScale up to a maximum

//
// Sprite Sheet Info
//

// Sprite Mapping (x coord):
//   0: Vertical
//   4: Horizontal
//   8: Up-Right
//  12: Down-Right
//  16: Down-Left
//  20: Up-Left

static const SDL_Rect sSpriteRect = {0, 0, 4, 4}; // First value is x coord
static const SDL_Rect sDestRect   = {0, 0, 4, 4};

static HilbertGraph_t       *sGraph;
static HilbertAlgoContext_t *sAlgo;

// HC API
static HilbertGraph_t *HC_Graph_Create(int order);
static void HC_Graph_Destroy(HilbertGraph_t *graph);

static HilbertAlgoContext_t *HC_Algo_Create(HilbertGraph_t *graph);
static bool HC_Algo_Advance(HilbertAlgoContext_t *algo);
static void HC_Algo_Complete(HilbertAlgoContext_t *algo);
static void HC_Algo_Destroy(HilbertAlgoContext_t *algo);

static HilbertCell_t HC_Get_Cell_By_Dirs(HilbertDir_t from, HilbertDir_t to);
static const char *HC_Get_Prod_Str(HilbertProdSym_t sym);

static HilbertCell_t HC_Get_Cell_By_Dirs(HilbertDir_t from, HilbertDir_t to)
{
    // TODO: Handle this in a more intelligent way by using a tilemap that has
    //       bits that determine whether or not a "side" is active, makes it
    //       incredibly easy to calculate the tile in question

    // TODO: Can also eliminate half these switch statements by making sure 'from'
    //       and 'to' are normalized.

    switch(from)
    {
        case HC_DIR_UP:
            switch(to)
            {
                //case HC_DIR_UP:
                case HC_DIR_DOWN:   return HC_CELL_VERT;
                case HC_DIR_LEFT:   return HC_CELL_U_L;
                case HC_DIR_RIGHT:  return HC_CELL_U_R;

                default:
                    break;
            }
            break;

        case HC_DIR_DOWN:
            switch(to)
            {
                case HC_DIR_UP:     return HC_CELL_VERT;
                //case HC_DIR_DOWN:
                case HC_DIR_LEFT:   return HC_CELL_D_L;
                case HC_DIR_RIGHT:  return HC_CELL_D_R;

                default:
                    break;
            }
            break;

        case HC_DIR_LEFT:
            switch(to)
            {
                case HC_DIR_UP:     return HC_CELL_U_L;
                case HC_DIR_DOWN:   return HC_CELL_D_L;
                //case HC_DIR_LEFT:
                case HC_DIR_RIGHT:  return HC_CELL_HORZ;

                default:
                    break;
            }
            break;

        case HC_DIR_RIGHT:
            switch(to)
            {
                case HC_DIR_UP:     return HC_CELL_U_R;
                case HC_DIR_DOWN:   return HC_CELL_D_R;
                case HC_DIR_LEFT:   return HC_CELL_HORZ;
                //case HC_DIR_RIGHT:

                default:
                    break;
            }
            break;

        default:
            break;
    }

    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized direction combination: %d -> %d", from, to);
    return HC_CELL_NONE;
}

static const char *HC_Get_Prod_Str(HilbertProdSym_t sym)
{
    switch(sym)
    {
        case HC_SYM_SUB_A:
            return HC_ALGO_PROD_A;

        case HC_SYM_SUB_B:
            return HC_ALGO_PROD_B;

        default:
            return NULL;
    }
}

// Helper Functions
static int init();
static void quit();

static int init()
{
    int ret;

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Initializing SDL.");
    ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

    if(ret < 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not initialize SDL: %s", SDL_GetError());
        return ret;
    }

    sSDLInited = true;

    // TODO: Create window smaller if needed
    winW = pMaxW;
    winH = pMaxH;

    // Create the window and the renderer.

    sWindow = SDL_CreateWindow("Hilbert Curve App", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winW, winH, 0);
    if(sWindow == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create window: %s", SDL_GetError());
        return 1;
    }

    sRenderer = SDL_CreateRenderer(sWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    if(sRenderer == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create renderer: %s", SDL_GetError());
        return 1;
    }

    // Load the tile map into a texture.

    sSpriteSurface = SDL_LoadBMP("res/sprites.bmp");
    if(sSpriteSurface == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load image file: %s", SDL_GetError());
        return 1;
    }

    sSpriteTexture = SDL_CreateTextureFromSurface(sRenderer, sSpriteSurface);
    if(sSpriteTexture == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create texture: %s", SDL_GetError());
        return 1;
    }

    sScreenTexture = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, winW, winH);
    if(sScreenTexture == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create screen texture: %s", SDL_GetError());
    }

    // Create any other contexts needed to track the Hilbert Curve.

    sGraph = HC_Graph_Create(pOrder);
    if(sGraph == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create graph.");
        return 1;
    }

    sAlgo = HC_Algo_Create(sGraph);
    if(sAlgo == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create algo.");
        return 1;
    }

    return 0;
}

static void cleanup()
{
    if(sAlgo != NULL)
    {
        HC_Algo_Destroy(sAlgo);
    }

    if(sGraph != NULL)
    {
        HC_Graph_Destroy(sGraph);
    }

    if(sScreenTexture != NULL)
    {
        SDL_DestroyTexture(sScreenTexture);
    }

    if(sSpriteTexture != NULL)
    {
        SDL_DestroyTexture(sSpriteTexture);
    }

    if(sSpriteSurface != NULL)
    {
        SDL_FreeSurface(sSpriteSurface);
    }

    if(sRenderer != NULL)
    {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Destroying renderer.");
        SDL_DestroyRenderer(sRenderer);
    }

    if(sWindow != NULL)
    {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Destroying window.");
        SDL_DestroyWindow(sWindow);
    }

    if(sSDLInited)
    {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Shutting down SDL.");
        SDL_Quit();
    }
}

int main(int argc, char **argv)
{
    SDL_Event ev;
    bool running = true;
    int ret;

    // TODO: Command-line parsing (curve order, tick rate, etc)
    (void)argc;
    (void)argv;

    ret = init();
    if(ret != 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Initialization failed: %d", ret);
        cleanup();
        return ret;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SDL Initialized.");

    while(running)
    {
        // The event loop is only used to determine if ESC or the exit button have
        // been pressed. All other events are discarded.
        while(SDL_PollEvent(&ev))
        {
            switch(ev.type)
            {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_KEYUP:
                    if(ev.key.keysym.sym == SDLK_ESCAPE)
                    {
                        running = false;
                    }
                    break;
            }
        }

        // Calculate the next step of the algorithm (this also updates the texture).
        HC_Algo_Advance(sAlgo);

        // Render the curve to the screen.
        SDL_SetRenderTarget(sRenderer, NULL);
        SDL_RenderClear(sRenderer);
        SDL_RenderCopy(sRenderer, sScreenTexture, NULL, NULL);
        SDL_RenderPresent(sRenderer);
        SDL_Delay(10);
    }

    cleanup();
    return 0;
}

static HilbertGraph_t *HC_Graph_Create(int order)
{
    HilbertGraph_t *graph;

    // Ensure our sidelen and number of cells will fit in a signed int
    if(order < 1 || order > 15)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "HC_Graph_Create: order too large");
        return NULL;
    }

    graph = (HilbertGraph_t *) malloc(sizeof(HilbertGraph_t));
    if(graph)
    {
        memset(graph, 0, sizeof(HilbertGraph_t));
        graph->order = order;
        graph->sidelen = 1 << order;

        graph->cells = (HilbertCell_t *) calloc((graph->sidelen * graph->sidelen), sizeof(HilbertCell_t));
        if(graph->cells == NULL)
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "HC_Graph_Create: could not alloc cells");
            free(graph);
            graph = NULL;
        }
    }
    else
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "HC_Graph_Create: could not alloc struct");
    }

    return graph;
}

static void HC_Graph_Destroy(HilbertGraph_t *graph)
{
    if(graph != NULL)
    {
        free(graph->cells);
        free(graph);
    }
}

static HilbertAlgoContext_t *HC_Algo_Create(HilbertGraph_t *graph)
{
    HilbertAlgoContext_t *algo;

    algo = (HilbertAlgoContext_t *) malloc(sizeof(HilbertAlgoContext_t));
    if(algo != NULL)
    {
        memset(algo, 0, sizeof(HilbertAlgoContext_t));

        algo->graph = graph;

        // Set the graph to start in the bottom-left.
        algo->x       = 0;
        algo->y       = algo->graph->sidelen - 1;
        algo->prevDir = HC_DIR_DOWN;

        algo->state = (HilbertAlgoRecurse_t *) calloc(graph->order, sizeof(HilbertAlgoRecurse_t));
        if(algo->state != NULL)
        {
            // Initialize the first production rule.
            algo->state[0].prod = HC_Get_Prod_Str(HC_SYM_SUB_A);
            algo->state[0].prod_index = 0;
        }
        else
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "HC_Algo_Create: could not alloc state");
            free(algo);
            algo = NULL;
        }
    }
    else
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "HC_Algo_Create: could not alloc struct");
    }

    return algo;
}

static bool HC_Algo_Advance(HilbertAlgoContext_t *algo)
{
    HilbertAlgoRecurse_t *state;
    bool                  advance_continue;

    // Make sure the algo object is valid.
    if(algo == NULL)
    {
        return true;
    }

    // Get the current recursion state of the context.
    state = &algo->state[algo->state_index];

    // Make sure we are not already finished.
    if(state->prod == NULL)
    {
        return true;
    }

    advance_continue = true;

    // Loop until we have done at least one action to update the graph.
    do
    {
        HilbertProdSym_t sym;

        // Get the next character and determine what to do.
        sym = (HilbertProdSym_t) state->prod[state->prod_index];
        state->prod_index++;

        switch(sym)
        {
            //
            // Stack-Affecting symbols (push/pop)
            //

            case HC_SYM_SUB_A:
            case HC_SYM_SUB_B:

                // Only recurse if we haven't reached the bottom.
                if(algo->state_index < (algo->graph->order - 1))
                {
                    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Graph [%2d]: %c", algo->state_index, sym);

                    // Push our current state on the state stack.
                    algo->state_index++;
                    state = &algo->state[algo->state_index];

                    // Set up the new state.
                    state->prod = HC_Get_Prod_Str(sym);
                    state->prod_index = 0;
                }
                else
                {
                    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Graph [%2d]: SKIP %c", algo->state_index, sym);
                }
                break;

            case HC_SYM_NULL:
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Graph [%2d]: NULL", algo->state_index);

                if(algo->state_index > 0)
                {
                    // Clear our state and move up in the stack.
                    memset(state, 0, sizeof(HilbertAlgoRecurse_t));
                    algo->state_index--;
                    state = &algo->state[algo->state_index];
                }
                else
                {
                    // We are at the top of the stack and reached the end of
                    // the production rules, so we are finished.
                    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Graph [%2d]: DONE", algo->state_index);

                    // There is no 'FORWARD' rule for the last cell so update it.
                    algo->graph->cells[(algo->y * algo->graph->sidelen) + algo->x] = HC_Get_Cell_By_Dirs(algo->prevDir, algo->currDir);

                    // Mark the production rule as being NULL.
                    state->prod = NULL;
                    advance_continue = false;
                }

                break;

            //
            // Graph-Affecting Symbols
            //

            case HC_SYM_LEFT:
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Graph [%2d]: LEFT", algo->state_index);
                algo->currDir = (HilbertDir_t) (((algo->currDir - 1) + HC_NUM_DIRS) % HC_NUM_DIRS);
                break;

            case HC_SYM_RIGHT:
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Graph [%2d]: RIGHT", algo->state_index);
                algo->currDir = (HilbertDir_t) ((algo->currDir + 1) % HC_NUM_DIRS);
                break;

            case HC_SYM_FORWARD: {

                size_t idx = (algo->y * algo->graph->sidelen) + algo->x;

                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Graph [%2d]: FORWARD from: (%d, %d) sl=%d idx=%lu", algo->state_index, algo->x, algo->y, algo->graph->sidelen, idx);

                // Update the grid based on which direction we decided to go.
                algo->graph->cells[idx] = HC_Get_Cell_By_Dirs(algo->prevDir, algo->currDir);

                SDL_Rect srcRect = sSpriteRect;
                SDL_Rect destRect;

                destRect.x = algo->x * 4 * pScale;
                destRect.y = algo->y * 4 * pScale;
                destRect.w = 4 * pScale;
                destRect.h = 4 * pScale;

                srcRect.x = (algo->graph->cells[idx] - 1) * 4;

                SDL_SetRenderTarget(sRenderer, sScreenTexture);
                SDL_RenderCopy(sRenderer, sSpriteTexture, &srcRect, &destRect);

                // Move the cursor.
                switch(algo->currDir)
                {
                    case HC_DIR_UP:
                        algo->y--;
                        break;

                    case HC_DIR_DOWN:
                        algo->y++;
                        break;

                    case HC_DIR_LEFT:
                        algo->x--;
                        break;

                    case HC_DIR_RIGHT:
                        algo->x++;
                        break;

                    default:
                        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Graph [%2d]: Unrecognized direction %d", algo->state_index, algo->currDir);
                        break;
                }

                // Ensure we are on a blank tile (otherwise the algorithm has
                // an error, since we cannot cross our own path.
                if(algo->graph->cells[(algo->y * algo->graph->sidelen) + algo->x] != HC_CELL_NONE)
                {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Graph [%2d]: Non-blank cell at (%d,%d)", algo->state_index, algo->x, algo->y);
                }

                // Cache the direction we were coming from.
                // TODO: Optimize this, this is hacky
                switch(algo->currDir)
                {
                    case HC_DIR_RIGHT: algo->prevDir = HC_DIR_LEFT;  break;
                    case HC_DIR_LEFT:  algo->prevDir = HC_DIR_RIGHT; break;
                    case HC_DIR_DOWN:  algo->prevDir = HC_DIR_UP;    break;
                    case HC_DIR_UP:    algo->prevDir = HC_DIR_DOWN;  break;

                    default:
                        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Unknown dir %d", algo->currDir);
                        break;
                }

                advance_continue = false;
                break;
            }
        }

    } while(advance_continue);

    return state->prod == NULL;
}

static void HC_Algo_Complete(HilbertAlgoContext_t *algo)
{
    if(algo)
    {
        while(!HC_Algo_Advance(algo))
        {
            // Wait until advance returns true (i.e. completed)
        }
    }
}

static void HC_Algo_Destroy(HilbertAlgoContext_t *algo)
{
    if(algo != NULL)
    {
        free(algo->state);
        free(algo);
    }
}
