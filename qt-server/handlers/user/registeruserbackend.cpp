#include "registeruserbackend.h"

#include "registerorderhandlers.h"
#include "registerfavoritehandlers.h"
#include "registerstationhandlers.h"
#include "registeruserhandlers.h"
#include "orderhandler.h"
#include "favoritehandler.h"
#include "stationhandler.h"
#include "userhandler.h"
#include "map/mapadapter.h"
#include "repositories/orderrepository.h"
#include "repositories/favoriterepository.h"
#include "repositories/predictionrepository.h"
#include "repositories/stationrepository.h"
#include "repositories/userrepository.h"
#include "services/user/orderservice.h"
#include "services/user/favoriteservice.h"
#include "services/user/stationservice.h"
#include "services/user/userservice.h"

class UserBackendRegistry::Impl
{
public:
    Impl(DatabaseManager *databaseManager, SessionManager *sessions,
         MessageDispatcher *dispatcher, const QString &avatarDirectory,
         const QString &mapApiKey, const QString &mapSigningSecret, DeviceControlService *deviceControl)
        : mapAdapter(mapApiKey, mapSigningSecret),
          userService(databaseManager, &userRepository, avatarDirectory),
          userHandler(&userService, sessions),
          stationService(databaseManager, &stationRepository, &predictionRepository, &mapAdapter,
                         &favoriteRepository),
          stationHandler(&stationService),
          favoriteService(databaseManager, &favoriteRepository, &userRepository, &stationRepository),
          favoriteHandler(&favoriteService),
          orderService(databaseManager, &userRepository, &orderRepository, deviceControl), orderHandler(&orderService)
    {
        registerUserHandlers(dispatcher, &userHandler);
        registerStationHandlers(dispatcher, &stationHandler);
        registerFavoriteHandlers(dispatcher, &favoriteHandler);
        registerOrderHandlers(dispatcher, &orderHandler);
    }
    UserRepository userRepository;
    StationRepository stationRepository;
    FavoriteRepository favoriteRepository;
    OrderRepository orderRepository;
    PredictionRepository predictionRepository;
    MapAdapter mapAdapter;
    UserService userService;
    UserHandler userHandler;
    StationService stationService;
    StationHandler stationHandler;
    FavoriteService favoriteService;
    FavoriteHandler favoriteHandler;
    OrderService orderService;
    OrderHandler orderHandler;
};

UserBackendRegistry::UserBackendRegistry(DatabaseManager *databaseManager, SessionManager *sessions,
                                         MessageDispatcher *dispatcher,
                                         const QString &avatarDirectory,
                                         const QString &mapApiKey,
                                         const QString &mapSigningSecret, DeviceControlService *deviceControl)
    : m_impl(new Impl(databaseManager, sessions, dispatcher, avatarDirectory, mapApiKey,
                      mapSigningSecret, deviceControl))
{
}
