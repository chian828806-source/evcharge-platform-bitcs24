#include "registeruserbackend.h"

#include "registerorderhandlers.h"
#include "registerstationhandlers.h"
#include "registeruserhandlers.h"
#include "orderhandler.h"
#include "stationhandler.h"
#include "userhandler.h"
#include "map/mapadapter.h"
#include "repositories/orderrepository.h"
#include "repositories/predictionrepository.h"
#include "repositories/stationrepository.h"
#include "repositories/userrepository.h"
#include "services/user/orderservice.h"
#include "services/user/stationservice.h"
#include "services/user/userservice.h"

class UserBackendRegistry::Impl
{
public:
    Impl(DatabaseManager *databaseManager, SessionManager *sessions,
         MessageDispatcher *dispatcher, const QString &avatarDirectory,
         const QString &mapApiKey, const QString &mapSigningSecret)
        : mapAdapter(mapApiKey, mapSigningSecret),
          userService(databaseManager, &userRepository, avatarDirectory),
          userHandler(&userService, sessions),
          stationService(databaseManager, &stationRepository, &predictionRepository, &mapAdapter),
          stationHandler(&stationService),
          orderService(databaseManager, &userRepository, &orderRepository), orderHandler(&orderService)
    {
        registerUserHandlers(dispatcher, &userHandler);
        registerStationHandlers(dispatcher, &stationHandler);
        registerOrderHandlers(dispatcher, &orderHandler);
    }
    UserRepository userRepository;
    StationRepository stationRepository;
    OrderRepository orderRepository;
    PredictionRepository predictionRepository;
    MapAdapter mapAdapter;
    UserService userService;
    UserHandler userHandler;
    StationService stationService;
    StationHandler stationHandler;
    OrderService orderService;
    OrderHandler orderHandler;
};

UserBackendRegistry::UserBackendRegistry(DatabaseManager *databaseManager, SessionManager *sessions,
                                         MessageDispatcher *dispatcher,
                                         const QString &avatarDirectory,
                                         const QString &mapApiKey,
                                         const QString &mapSigningSecret)
    : m_impl(new Impl(databaseManager, sessions, dispatcher, avatarDirectory, mapApiKey,
                      mapSigningSecret))
{
}
